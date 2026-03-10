#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

ENCRYPTION_TYPES=(
    "AES_256_GCM"
    "AES_256_ECB"
    "AES_256_CBC"
    "AES_256_CFB"
    "AES_256_OFB"
    "AES_256_CTR"
    "SM4_ECB"
    "SM4_CBC"
    "SM4_CTR"
)

create_config() {
    local enc_type=$1
    cat > "$PROJECT_DIR/conf/dfc.conf" << EOF
Server DFS1 127.0.0.1:10001
Server DFS2 127.0.0.1:10002
Server DFS3 127.0.0.1:10003
Server DFS4 127.0.0.1:10004

Username: Bob
Password: ComplextPassword
EncryptionType: $enc_type
EOF
}

test_encryption() {
    local enc_type=$1
    local test_name="test_${enc_type}"
    
    echo ""
    echo "========================================"
    echo "Testing encryption: $enc_type"
    echo "========================================"
    
    create_config "$enc_type"
    
    echo "Test content for $enc_type encryption" > "$SCRIPT_DIR/${test_name}.txt"
    
    echo "Testing PUT with $enc_type..."
    {
        sleep 1
        echo "PUT $SCRIPT_DIR/${test_name}.txt ${test_name}_remote.txt"
        sleep 3
        echo "EXIT"
    } | timeout 15s "$PROJECT_DIR/bin/dfc" "$PROJECT_DIR/conf/dfc.conf" > /dev/null 2>&1
    
    local put_success=false
    for i in 1 2 3 4; do
        if [ -f "$PROJECT_DIR/server/DFS$i/Bob/.${test_name}_remote.txt.0" ] || \
           [ -f "$PROJECT_DIR/server/DFS$i/Bob/.${test_name}_remote.txt.1" ]; then
            put_success=true
            break
        fi
    done
    
    if $put_success; then
        echo "  PUT: PASSED"
    else
        echo "  PUT: FAILED"
        rm -f "$SCRIPT_DIR/${test_name}.txt"
        return 1
    fi
    
    echo "Testing GET with $enc_type..."
    {
        sleep 1
        echo "GET ${test_name}_remote.txt $SCRIPT_DIR/${test_name}_downloaded.txt"
        sleep 3
        echo "EXIT"
    } | timeout 15s "$PROJECT_DIR/bin/dfc" "$PROJECT_DIR/conf/dfc.conf" > /dev/null 2>&1
    
    if [ -f "$SCRIPT_DIR/${test_name}_downloaded.txt" ]; then
        local original=$(cat "$SCRIPT_DIR/${test_name}.txt")
        local downloaded=$(cat "$SCRIPT_DIR/${test_name}_downloaded.txt")
        
        if [ "$original" = "$downloaded" ]; then
            echo "  GET: PASSED (content verified)"
        else
            echo "  GET: FAILED (content mismatch)"
            echo "    Original: $original"
            echo "    Downloaded: $downloaded"
            rm -f "$SCRIPT_DIR/${test_name}.txt" "$SCRIPT_DIR/${test_name}_downloaded.txt"
            return 1
        fi
    else
        echo "  GET: FAILED (file not downloaded)"
        rm -f "$SCRIPT_DIR/${test_name}.txt"
        return 1
    fi
    
    rm -f "$SCRIPT_DIR/${test_name}.txt" "$SCRIPT_DIR/${test_name}_downloaded.txt"
    return 0
}

echo "========================================"
echo "  DFS Encryption Algorithm Tests"
echo "========================================"

cd "$PROJECT_DIR" || exit 1

if [ ! -f "$PROJECT_DIR/bin/dfc" ]; then
    echo "Building DFS..."
    make dfs dfc > /dev/null 2>&1
fi

make kill > /dev/null 2>&1
sleep 1
cd "$PROJECT_DIR" || exit 1

rm -rf server/DFS*/*
mkdir -p server/DFS1 server/DFS2 server/DFS3 server/DFS4

"$PROJECT_DIR/bin/dfs" server/DFS1 10001 --no-debug &
PID1=$!
"$PROJECT_DIR/bin/dfs" server/DFS2 10002 --no-debug &
PID2=$!
"$PROJECT_DIR/bin/dfs" server/DFS3 10003 --no-debug &
PID3=$!
"$PROJECT_DIR/bin/dfs" server/DFS4 10004 --no-debug &
PID4=$!

sleep 3

if ! kill -0 $PID1 2>/dev/null || ! kill -0 $PID2 2>/dev/null || ! kill -0 $PID3 2>/dev/null || ! kill -0 $PID4 2>/dev/null; then
    echo "Failed to start servers"
    exit 1
fi

passed=0
failed=0
failed_types=""

for enc_type in "${ENCRYPTION_TYPES[@]}"; do
    if test_encryption "$enc_type"; then
        ((passed++))
    else
        ((failed++))
        failed_types="$failed_types $enc_type"
    fi
done

echo ""
echo "========================================"
echo "Test Results: $passed passed, $failed failed"
if [ -n "$failed_types" ]; then
    echo "Failed types:$failed_types"
fi
echo "========================================"

create_config "AES_256_ECB"

exit $failed
