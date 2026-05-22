#!/usr/bin/env python3
import argparse
import json
import os
import re
import sys
import urllib.error
import urllib.request


NOTION_API = "https://api.notion.com/v1"


def notion_request(method, path, token, version, body=None):
    data = None
    headers = {
        "Authorization": f"Bearer {token}",
        "Notion-Version": version,
        "Content-Type": "application/json",
    }
    if body is not None:
        data = json.dumps(body).encode("utf-8")

    request = urllib.request.Request(
        f"{NOTION_API}{path}",
        data=data,
        headers=headers,
        method=method,
    )

    try:
        with urllib.request.urlopen(request) as response:
            raw = response.read().decode("utf-8")
            return json.loads(raw) if raw else {}
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"Notion API error {exc.code} {method} {path}: {detail}") from exc


def chunks(items, size):
    for index in range(0, len(items), size):
        yield items[index : index + size]


def rich_text(text):
    if not text:
        return []

    parts = []
    for index in range(0, len(text), 1900):
        parts.append(
            {
                "type": "text",
                "text": {"content": text[index : index + 1900]},
            }
        )
    return parts


def text_block(block_type, text):
    return {
        "object": "block",
        "type": block_type,
        block_type: {"rich_text": rich_text(text)},
    }


def code_block(text, language="plain text"):
    notion_language = {
        "text": "plain text",
        "plain": "plain text",
        "c++": "c++",
        "cpp": "c++",
        "bash": "bash",
        "sh": "shell",
    }.get(language.strip().lower(), language.strip().lower() or "plain text")

    return {
        "object": "block",
        "type": "code",
        "code": {
            "rich_text": rich_text(text),
            "language": notion_language,
        },
    }


def split_table_row(line):
    line = line.strip()
    if line.startswith("|"):
        line = line[1:]
    if line.endswith("|"):
        line = line[:-1]
    return [cell.strip() for cell in line.split("|")]


def is_table_separator(line):
    cells = split_table_row(line)
    return bool(cells) and all(re.fullmatch(r":?-{3,}:?", cell.strip()) for cell in cells)


def table_block(lines):
    rows = [split_table_row(line) for line in lines if not is_table_separator(line)]
    if not rows:
        return None

    width = max(len(row) for row in rows)
    table_rows = []
    for row in rows:
        padded = row + [""] * (width - len(row))
        table_rows.append(
            {
                "object": "block",
                "type": "table_row",
                "table_row": {"cells": [rich_text(cell) for cell in padded]},
            }
        )

    return {
        "object": "block",
        "type": "table",
        "table": {
            "table_width": width,
            "has_column_header": len(rows) > 1,
            "has_row_header": False,
            "children": table_rows,
        },
    }


def flush_paragraph(lines, blocks):
    if not lines:
        return
    blocks.append(text_block("paragraph", " ".join(line.strip() for line in lines).strip()))
    lines.clear()


def markdown_to_blocks(markdown):
    blocks = []
    paragraph = []
    table_lines = []
    code_lines = []
    code_language = "plain text"
    in_code = False
    skipped_title = False

    lines = markdown.splitlines()
    for line in lines:
        fence = re.match(r"^```(.*)$", line)
        if fence:
            if in_code:
                blocks.append(code_block("\n".join(code_lines), code_language))
                code_lines = []
                code_language = "plain text"
                in_code = False
            else:
                flush_paragraph(paragraph, blocks)
                if table_lines:
                    block = table_block(table_lines)
                    if block:
                        blocks.append(block)
                    table_lines = []
                code_language = fence.group(1) or "plain text"
                in_code = True
            continue

        if in_code:
            code_lines.append(line)
            continue

        if "|" in line and line.strip().startswith("|"):
            flush_paragraph(paragraph, blocks)
            table_lines.append(line)
            continue

        if table_lines:
            block = table_block(table_lines)
            if block:
                blocks.append(block)
            table_lines = []

        if not line.strip():
            flush_paragraph(paragraph, blocks)
            continue

        heading = re.match(r"^(#{1,3})\s+(.+)$", line)
        if heading:
            flush_paragraph(paragraph, blocks)
            level = len(heading.group(1))
            text = heading.group(2).strip()
            if level == 1 and not skipped_title:
                skipped_title = True
                continue
            block_type = {1: "heading_1", 2: "heading_2", 3: "heading_3"}[level]
            blocks.append(text_block(block_type, text))
            continue

        bullet = re.match(r"^[-*]\s+(.+)$", line)
        if bullet:
            flush_paragraph(paragraph, blocks)
            blocks.append(text_block("bulleted_list_item", bullet.group(1).strip()))
            continue

        numbered = re.match(r"^\d+\.\s+(.+)$", line)
        if numbered:
            flush_paragraph(paragraph, blocks)
            blocks.append(text_block("numbered_list_item", numbered.group(1).strip()))
            continue

        paragraph.append(line)

    if in_code:
        blocks.append(code_block("\n".join(code_lines), code_language))
    if table_lines:
        block = table_block(table_lines)
        if block:
            blocks.append(block)
    flush_paragraph(paragraph, blocks)
    return blocks


def clear_page(page_id, token, version):
    cursor = None
    while True:
        query = "?page_size=100"
        if cursor:
            query += f"&start_cursor={cursor}"
        response = notion_request("GET", f"/blocks/{page_id}/children{query}", token, version)
        for block in response.get("results", []):
            notion_request("PATCH", f"/blocks/{block['id']}", token, version, {"archived": True})
        if not response.get("has_more"):
            break
        cursor = response.get("next_cursor")


def append_blocks(page_id, blocks, token, version):
    for group in chunks(blocks, 100):
        notion_request(
            "PATCH",
            f"/blocks/{page_id}/children",
            token,
            version,
            {"children": group},
        )


def main():
    parser = argparse.ArgumentParser(description="Sync a Markdown file to a Notion page.")
    parser.add_argument("markdown_file")
    parser.add_argument("--page-id", default=os.environ.get("NOTION_PAGE_ID"))
    parser.add_argument("--token", default=os.environ.get("NOTION_TOKEN"))
    parser.add_argument("--version", default=os.environ.get("NOTION_VERSION", "2022-06-28"))
    args = parser.parse_args()

    if not args.page_id:
        print("NOTION_PAGE_ID is required.", file=sys.stderr)
        return 2
    if not args.token:
        print("NOTION_TOKEN is required.", file=sys.stderr)
        return 2

    with open(args.markdown_file, "r", encoding="utf-8") as source:
        markdown = source.read()

    blocks = markdown_to_blocks(markdown)
    clear_page(args.page_id, args.token, args.version)
    append_blocks(args.page_id, blocks, args.token, args.version)
    print(f"Synced {len(blocks)} blocks to Notion page {args.page_id}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
