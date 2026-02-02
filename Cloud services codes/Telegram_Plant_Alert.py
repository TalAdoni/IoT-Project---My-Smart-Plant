import json
import urllib3
import boto3
from boto3.dynamodb.conditions import Key
from decimal import Decimal
import os

# --- Configuration ---
# SECURITY: These values are loaded from Environment Variables in Lambda Configuration
TELEGRAM_TOKEN = os.environ.get("TELEGRAM_TOKEN", "YOUR_BOT_TOKEN_HERE")
GROUP_CHAT_ID = os.environ.get("GROUP_CHAT_ID", "YOUR_CHAT_ID_HERE")
DYNAMODB_TABLE = os.environ.get("TABLE_NAME", "PlantData")

# --- Calibration Constants ---
MAX_DRY = 2600
MIN_WET = 995
DIVIDER = 16.05

http = urllib3.PoolManager()

def send_telegram(text):
    """Sends a message to the configured Telegram group."""
    url = f"https://api.telegram.org/bot{TELEGRAM_TOKEN}/sendMessage"
    payload = {"chat_id": GROUP_CHAT_ID, "text": text}
    encoded_data = json.dumps(payload).encode('utf-8')
    try:
        res = http.request('POST', url, body=encoded_data, headers={'Content-Type': 'application/json'})
        print(f"Telegram Response: {res.status}")
    except Exception as e:
        print(f"Telegram Connection Error: {e}")

def get_val(image, field):
    """Safely extracts a value whether it's stored as Number (N) or String (S)."""
    data = image.get(field, {})
    return data.get('N') or data.get('S')

def get_moisture(raw_val):
    """Calculates moisture percentage from raw sensor value."""
    if raw_val is None: return None
    moisture = (MAX_DRY - int(raw_val)) / DIVIDER
    return max(0, min(100, moisture))

def get_state(pct):
    if pct is None: return "UNKNOWN"
    if pct < 10: return "CRITICAL"
    if pct < 30: return "MODERATE"
    return "NORMAL"

def lambda_handler(event, context):
    db = boto3.resource('dynamodb')
    table = db.Table(DYNAMODB_TABLE)
    
    print(f"Received event with {len(event['Records'])} records")
    
    for record in event['Records']:
        if record['eventName'] in ['INSERT', 'MODIFY']:
            new_img = record['dynamodb'].get('NewImage', {})
            thing_name = new_img.get('thing', {}).get('S', 'unknown')
            
            plant_id = get_val(new_img, 'plantid') or get_val(new_img, 'plantId') or "unknown"
            current_time = int(get_val(new_img, 'epochMs') or 0)

            current_raw = get_val(new_img, 'soilRaw')
            new_pct = get_moisture(current_raw)
            if new_pct is None:
                continue

            try:
                response = table.query(
                    KeyConditionExpression=Key('thing').eq(thing_name),
                    ScanIndexForward=False, 
                    Limit=50, 
                    ConsistentRead=True 
                )
            except Exception as e:
                print(f"Query failed: {e}")
                continue

            items = response.get('Items', [])
            old_pct = new_pct
            
            plant_history = [it for it in items if (it.get('plantid') or it.get('plantId')) == plant_id]
            
            prev_item = None
            for item in plant_history:
                item_time = int(item.get('epochMs', 0))
                if item_time < current_time:
                    prev_item = item
                    break
            
            if prev_item:
                prev_raw = prev_item.get('soilRaw')
                calculated_old = get_moisture(prev_raw)
                if calculated_old is not None:
                    old_pct = calculated_old

            new_state = get_state(new_pct)
            old_state = get_state(old_pct)
            
            print(f"Plant: {plant_id} | Old: {old_state} ({old_pct:.1f}%) -> New: {new_state} ({new_pct:.1f}%)")
            
            if new_state != old_state:
                print(f"State change detected. Sending Telegram...")
                pct_display = int(new_pct)
                if new_state == "CRITICAL":
                    send_telegram(f"🚨 Plant {plant_id}: CRITICAL ({pct_display}%) < 10%. Water NOW!")
                elif new_state == "MODERATE":
                    send_telegram(f"💧 Plant {plant_id}: LOW ({pct_display}%) 10-30%. Water soon.")
                elif new_state == "NORMAL":
                    send_telegram(f"✅ Plant {plant_id}: NORMAL ({pct_display}%) > 30%.")
                    
    return {'statusCode': 200}
