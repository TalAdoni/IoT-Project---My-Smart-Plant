import boto3
import time
import json
import os
from decimal import Decimal

def lambda_handler(event, context):
    # SECURITY FIX: Get the ARN from an Environment Variable
    # Do not hardcode account IDs in uploaded code
    ROLE_ARN = os.environ.get('ROLE_ARN')
    
    if not ROLE_ARN:
        print("Error: ROLE_ARN environment variable is missing.")
        return {"status": "error", "message": "Configuration error"}

    sts_client = boto3.client('sts')
    try:
        assumed_role = sts_client.assume_role(
            RoleArn=ROLE_ARN,
            RoleSessionName="IoTCrossAccountSession"
        )
    except Exception as e:
        print(f"Error assuming role: {e}")
        raise e
    
    creds = assumed_role['Credentials']
    dynamodb = boto3.resource(
        'dynamodb',
        aws_access_key_id=creds['AccessKeyId'],
        aws_secret_access_key=creds['SecretAccessKey'],
        aws_session_token=creds['SessionToken']
    )
    
    table = dynamodb.Table('PlantData')
    
    # Convert floats to Decimal for DynamoDB compatibility
    item = json.loads(json.dumps(event), parse_float=Decimal)
    
    if 'timestamp' not in item:
        item['timestamp'] = int(time.time())
    
    try:
        table.put_item(Item=item)
        print(f"Success! Data sent to DynamoDB: {item}")
    except Exception as e:
        print(f"Error writing to DynamoDB: {e}")
        raise e
        
    return {"status": "success"}