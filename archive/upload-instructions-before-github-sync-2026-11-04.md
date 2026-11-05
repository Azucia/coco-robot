> 历史首次上传说明，已由现有仓库同步覆盖。CURRENT_STATE.md优先。

# 换电脑上传GitHub

此ZIP是完整项目仓库，包含当前源码、锁定依赖、固件、文档和隐藏的`.git`本地提交。没有用户密码/token/NVS。**当前没有创建GitHub仓库，也没有推送。** 用户确认本机桌面账号不同，决定换电脑自行上传。

## 推荐：GitHub Desktop

1. 完整解压ZIP。进入`coco-robot`，确认里面有README.md、CURRENT_STATE.md、source、firmware及隐藏的.git文件夹。不要只从ZIP里拖出可见文件。
2. 在另一电脑的GitHub Desktop登录正确账号，选择File → Add local repository，选择解压出来的`coco-robot`文件夹。
3. 选择Publish repository，名称`coco-robot`，**勾选Keep this code private**。确认账号正确后发布。不要勾选忽略/排除已有source或firmware文件。
4. 发布完成后打开GitHub仓库网页，确认有CURRENT_STATE.md、source/voice、source/motor、source/remote、firmware及全部文档。
5. GitHub Desktop应没有待提交改动；main最新提交与本地`git log -1`一致。首次上传含完整依赖，文件较多，请等完成。

不建议把整个ZIP当一个文件上传：那样新对话不能直接逐个读取源码。GitHub网页手工逐个上传也不适合本仓库文件数量。请使用GitHub Desktop发布整个本地仓库。

## 已有GitHub CLI时

在解压的coco-robot目录打开终端，使用正确账号：

```powershell
gh auth login
gh repo create coco-robot --private --source . --remote origin --push
git rev-parse HEAD
git ls-remote origin refs/heads/main
```

最后两项提交号应一致。这些命令是给另一电脑执行的上传步骤，本机未执行创建/推送。若同名仓库已存在，先确认是否本项目，不能强制覆盖已有历史；改名或谨慎指定正确origin后普通push。

本地Git自动整理作者Coco Project Migration <coco-migration@localhost>只是迁移提交作者，不是用户GitHub账号。发布账号以GitHub Desktop/CLI实际登录为准。

## 上传后交给新对话

将仓库链接发给新对话，并要求依次读CURRENT_STATE.md、TODO.md、DECISIONS.md，再读hardware/wiring.md、software/setup.md和debug/verified_tests.md。明确CURRENT_STATE.md优先级最高；DO NOT USE中的旧方案不得默认采用。上传后把TODO的迁移任务标完成，并记录实际仓库地址和远端提交号。
