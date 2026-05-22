# Notion自動更新

`docs/notion/Pico_Tama_Project.md` を更新して `main` に push すると、GitHub Actions がNotionページを更新します。

## GitHub Secrets

リポジトリの `Settings` → `Secrets and variables` → `Actions` に次を追加します。

| Name | Value |
| --- | --- |
| `NOTION_TOKEN` | Notionインテグレーションのシークレット |
| `NOTION_PAGE_ID` | `368c7245a8ab812c9cf8db53bfc1dff0` |

## Notion側で必要なこと

Notionインテグレーションを作成し、同期先ページに接続します。

同期先ページ:
https://www.notion.so/368c7245a8ab812c9cf8db53bfc1dff0

## 手動実行

GitHub Actions の `Sync Notion` ワークフローは `workflow_dispatch` に対応しています。
GitHub上のActions画面から手動実行できます。
