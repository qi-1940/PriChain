#!/bin/bash

# 定义测试的模块顺序及其对应的dmesg输出行数
modules=(
    "rwsem:43"
    "rb_rwsem:57"
    "rwlock:40"
    "rb_rwmutex:115"
    "mutex:38"
    "rb_mutex:43"
)

# 清空或创建输出文件
> tests_output.txt

# 主测试循环
for entry in "${modules[@]}"; do
    module="${entry%:*}"
    lines="${entry#*:}"
    echo "===== 开始测试 $module =====" | tee -a tests_output.txt

    (
        echo "进入目录：$module"
        cd "$module" || exit 1

        # 清理并编译
        echo "执行：make clean && make"
        make clean && make || exit 1

        # 插入内核模块
        echo "插入模块：insmod ${module}_test.ko"
        if ! insmod "${module}_test.ko"; then
            echo "错误：插入模块失败！"
            exit 1
        fi

        # 等待5秒
        echo "等待5秒..."
        sleep 5

        # 移除模块
        #echo "移除模块：rmmod ${module}_test"
        #rmmod "${module}_test"

        # 获取内核日志
        
        echo "获取最后${lines}行dmesg输出"
        dmesg | tail -n $lines

        echo "返回上级目录"
    ) 2>&1 | tee -a tests_output.txt

    echo -e "===== 测试 $module 完成 =====\n" | tee -a tests_output.txt
done

echo "所有测试完成！结果已保存到 tests_output.txt"
