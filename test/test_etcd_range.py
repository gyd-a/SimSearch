import requests
import json
import base64

def prefix_range_end(prefix: str) -> str:
    """
    根据 etcd 前缀规则，生成 prefix 的 range_end。
    逻辑是将 prefix 的最后一个字节 +1。
    """
    b = bytearray(prefix.encode("utf-8"))
    for i in reversed(range(len(b))):
        if b[i] < 0xFF:
            b[i] += 1
            return b[:i+1].decode("latin1")
    return '\0'

def etcd_prefix_query(prefix: str):
    url = "http://localhost:2379/v3/kv/range"
    
    key_b64 = base64.b64encode(prefix.encode("utf-8")).decode("utf-8")
    range_end_str = prefix_range_end(prefix)
    range_end_b64 = base64.b64encode(range_end_str.encode("utf-8")).decode("utf-8")

    data = {
        "key": key_b64,
        "range_end": range_end_b64
    }

    headers = {
        "Content-Type": "application/json"
    }

    print(f"🔍 Prefix: {prefix}, range_end: {range_end_str} (Base64: {range_end_b64})")
    
    response = requests.post(url, data=json.dumps(data), headers=headers)

    if response.status_code != 200:
        print("❌ HTTP error:", response.status_code, response.text)
        return

    result = response.json()
    if "kvs" not in result:
        print("⚠️ No kvs found.")
        return

    for kv in result["kvs"]:
        key_str = base64.b64decode(kv["key"]).decode("utf-8")
        value_str = base64.b64decode(kv["value"]).decode("utf-8")
        print(f"✅ Key: {key_str}, Value: {value_str}")



# 调用示例
etcd_prefix_query("/spaces/mata")

