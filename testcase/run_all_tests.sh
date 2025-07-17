#!/bin/bash
set -e
cd "$(dirname "$0")"

# 编译所有 Java 测试用例
javac *.java

# 运行所有测试用例
JVM=../build/myJVMinCpp
if [ ! -x "$JVM" ]; then
    echo "JVM 可执行文件 $JVM 不存在或不可执行" >&2
    exit 1
fi

fail=0
for cls in *.java; do
    name="${cls%.java}"
    echo "===== Running $name ====="
    "$JVM" "$name" || fail=1
    echo
    echo "========================="
done

if [ $fail -eq 0 ]; then
    echo "所有测试用例运行完毕，无异常退出。"
else
    echo "部分测试用例运行异常，请检查输出。"
fi 