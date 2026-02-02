import boto3
import time
import requests
import os

# --- 1. הגדרות ---

ACCESS_KEY = os.getenv("AWS_ACCESS_KEY", "YOUR_ACCESS_KEY_HERE")
SECRET_KEY = os.getenv("AWS_SECRET_KEY", "YOUR_SECRET_KEY_HERE")

REGION = "us-east-1"  # Or your specific region
QUESTDB_URL = "http://localhost:9000/write?precision=ms"

is_first_run = True
sent_cache = set()

# חיבור ל-DynamoDB
dynamodb = boto3.resource('dynamodb', region_name=REGION, 
                          aws_access_key_id=ACCESS_KEY, 
                          aws_secret_access_key=SECRET_KEY)
table = dynamodb.Table('PlantData')

print("Bridge started.")

while True:
    try:
        # משתנים לטיפול ב-Pagination (דפדוף)
        last_evaluated_key = None
        total_records_processed = 0
        
        # לולאה שרצה כל עוד יש עוד "דפים" של מידע ב-DynamoDB
        while True:
            scan_kwargs = {}
            if last_evaluated_key:
                scan_kwargs['ExclusiveStartKey'] = last_evaluated_key

            # סריקה (מביאה רק 1MB בכל פעם)
            response = table.scan(**scan_kwargs)
            items = response.get('Items', [])
            
            data_payload = ""
            new_items_count_in_batch = 0
            
            # הגדרת חלון זמן (רק אם זו לא הריצה הראשונה)
            current_time_ms = int(time.time() * 1000)
            time_threshold = 0 if is_first_run else (current_time_ms - (60 * 60 * 1000))

            if is_first_run and total_records_processed == 0:
                print("First run: Loading ALL data from DynamoDB (Paginated)...")
            
            for item in items:
                epoch_ms = int(item.get('epochMs', 0))
                
                # סינון לפי זמן (רלוונטי רק החל מהריצה השנייה)
                if epoch_ms < time_threshold:
                    continue

                plant_id = item.get('plantid') or item.get('plantId') or "unknown"
                unique_id = f"{plant_id}_{epoch_ms}"
                
                # מניעת כפילויות
                if unique_id in sent_cache:
                    continue

                soil_raw = int(item.get('soilRaw', 0))
                temp_c = float(item.get('tempC', 0))
                hum_rh = float(item.get('humRH', 0))

                line = f"sensor_data,plant={plant_id} soilRaw={soil_raw}i,tempC={temp_c},humRH={hum_rh} {epoch_ms}"
                data_payload += line + "\n"
                
                sent_cache.add(unique_id)
                new_items_count_in_batch += 1

            # שליחה ל-QuestDB
            if data_payload:
                res = requests.post(QUESTDB_URL, data=data_payload)
                if res.status_code == 204:
                    total_records_processed += new_items_count_in_batch
                    if new_items_count_in_batch > 0:
                        print(f"[{time.strftime('%H:%M:%S')}] Batch synced: {new_items_count_in_batch} records.")
                else:
                    print(f"Error: {res.status_code}")
            
            # בדיקה אם יש עוד דפים ב-DynamoDB
            last_evaluated_key = response.get('LastEvaluatedKey')
            if not last_evaluated_key:
                break 

        if is_first_run:
            print(f"Initial load complete. Total records loaded: {total_records_processed}")
            is_first_run = False
            print("Now switching to incremental updates (last 60 mins).")

        if len(sent_cache) > 20000:
            sent_cache.clear()
            
        time.sleep(30)
        
    except Exception as e:
        print(f"Error: {e}")
        time.sleep(10)

