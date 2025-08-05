#!/bin/bash

# PriChain 优先级继承测试脚本
# 根据 tests_design.md 设计实现

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志文件
LOG_DIR="./test_log/$(date +%Y.%m.%d)"
LOG_FILE="$LOG_DIR/test_$(date +%H%M%S).log"
#KERNEL_LOG="/var/log/kern.log"

# 创建日志目录
mkdir -p "$LOG_DIR"

# 打印带颜色的消息
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 清理函数
cleanup() {
    print_info "清理测试环境..."
    
    # 卸载可能存在的测试模块
    rmmod test 2>/dev/null || true
    rmmod rwlock_test 2>/dev/null || true
    rmmod rwsem_test 2>/dev/null || true
    
    print_info "清理完成"
}

# 编译测试模块
compile_test_modules() {
    print_info "开始编译测试模块..."
    
    # 编译互斥锁测试模块
    print_info "编译互斥锁测试模块..."
    cd tests/互斥锁
    make clean >/dev/null 2>&1 || true
    if make all; then
        print_success "互斥锁测试模块编译成功"
    else
        print_error "互斥锁测试模块编译失败"
        exit 1
    fi
    cd ../..
    
    # 编译读写锁测试模块
    print_info "编译读写锁测试模块..."
    cd tests/读写锁/rwlock
    make clean >/dev/null 2>&1 || true
    if make all; then
        print_success "读写锁测试模块编译成功"
    else
        print_error "读写锁测试模块编译失败"
        exit 1
    fi
    cd ../../..
    
    # 编译读写信号量测试模块
    print_info "编译读写信号量测试模块..."
    cd tests/读写信号量/rwsem
    make clean >/dev/null 2>&1 || true
    if make all; then
        print_success "读写信号量测试模块编译成功"
    else
        print_error "读写信号量测试模块编译失败"
        exit 1
    fi
    cd ../../..
    
    print_success "所有测试模块编译完成"
}

# 运行单个测试模块
run_test_module() {
    local module_name=$1
    local module_path=$2
    local test_name=$3
    
    print_info "开始测试: $test_name"
    
    # 清空内核日志缓冲区
    dmesg -c >/dev/null 2>&1 || true
    
    insmod "$module_path"
    
    local ins_re=$?

    # 等待10秒让初始化函数运行完毕
    print_info "等待10秒让初始化函数运行完毕..."
    sleep 10
    
    if [ $ins_re==0 ]
    then
        echo "模块安装成功"
    else 
        echo "模块安装失败"
    fi 

    # 卸载模块
    print_info "卸载 $test_name 模块..."
    if rmmod "$module_name"; then
        print_success "$test_name 模块卸载成功"
    else
        print_warning "$test_name 模块卸载失败，可能仍在运行"
    fi
    
    # 获取内核日志
    print_info "获取内核日志..."
    dmesg | grep -E "(HIGHEST_PRIO_TEST|RWLOCK_TEST|RWSEM_TEST)" > "$LOG_DIR/${test_name}_$(date +%H%M%S).log" 2>/dev/null || true
    
    print_success "$test_name 测试完成"
    echo "----------------------------------------"
}

# 分析测试结果
analyze_results() {
    print_info "开始分析测试结果..."
    
    local log_files=($(ls "$LOG_DIR"/*.log 2>/dev/null | grep -v "$(basename "$LOG_FILE")" || true))
    
    if [ ${#log_files[@]} -eq 0 ]; then
        print_warning "未找到测试日志文件"
        return
    fi
    
    print_info "找到 ${#log_files[@]} 个测试日志文件:"
    
    for log_file in "${log_files[@]}"; do
        local test_name=$(basename "$log_file" | cut -d'_' -f1)
        print_info "分析 $test_name 测试结果..."
        
        # 检查是否有错误
        if grep -q "ERROR\|FAILED" "$log_file" 2>/dev/null; then
            print_error "$test_name 测试中发现错误"
        else
            print_success "$test_name 测试未发现明显错误"
        fi
        
        # 统计日志条目
        local line_count=$(wc -l < "$log_file" 2>/dev/null || echo "0")
        print_info "$test_name 测试产生 $line_count 条日志记录"
        
        # 显示关键日志信息
        echo "关键日志信息:"
        grep -E "(START|GOT LOCK|RELEASE LOCK|FINISHED|PRIORITY)" "$log_file" 2>/dev/null | head -10 || true
        echo ""
    done
}

# 主函数
main() {
    print_info "========================================"
    print_info "PriChain 优先级继承测试开始"
    print_info "测试时间: $(date)"
    print_info "========================================"
    
    # 检查是否以root权限运行
    if [ "$EUID" -ne 0 ]; then
        print_error "请以root权限运行此脚本"
        exit 1
    fi
    
    # 设置清理陷阱
    trap cleanup EXIT
    
    # 编译测试模块
    compile_test_modules
    
    # 运行互斥锁测试
    run_test_module "test" "tests/互斥锁/test.ko" "互斥锁测试"
    
    # 运行读写锁测试
    run_test_module "rwlock_test" "tests/读写锁/rwlock/rwlock_test.ko" "读写锁测试"
    
    # 运行读写信号量测试
    run_test_module "rwsem_test" "tests/读写信号量/rwsem/rwsem_test.ko" "读写信号量测试"
    
    # 分析测试结果
    analyze_results
    
    print_info "========================================"
    print_success "所有测试完成！"
    print_info "测试日志保存在: $LOG_DIR"
    print_info "========================================"
}

# 运行主函数
main "$@"
