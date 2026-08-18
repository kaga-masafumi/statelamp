# OpenClaw連携

このディレクトリにはStateLamp用のOpenClaw pluginが含まれています。StateLampの基準となる連携実装で、ライフサイクル状態の送信、操作者による承認イベントの監視、`human_required` と `human_required_clear` ツールを提供します。

人間への呼び出しを行うと、一意のattention IDが返ります。AgentはそのIDを保持し、要求された操作が完了した後に、そのIDだけを解除してください。Bridgeへ接続できない場合も、失敗を報告しつつ `human_action_still_required: true` を保持します。

OpenClawから到達できるBridge URLをpluginに設定してください。認証情報やマシン固有の設定は、このリポジトリの外で管理します。

テストは次のコマンドで実行できます。

```bash
npm install && npm test
```
