import requests
import json
import sys
import subprocess
import time
import pandas as pd
import matplotlib.pyplot as plt
from datetime import datetime
import numpy as np
import seaborn as sns

# Define URLs
SERVER_URL = "http://localhost:3000"
PROXY_URL = "http://localhost:8080"

# Data collection structures
performance_data = {
    'operation': [],
    'response_time': [],
    'status_code': [],
    'timestamp': [],
    'cache_hit': []
}

def get_local_ip():
    try:
        cmd = "hostname -I | awk '{print $1}'"
        local_ip = subprocess.check_output(cmd, shell=True).decode().strip()
        return local_ip if local_ip else "localhost"
    except:
        return "localhost"

def log_request(operation, response_time, status_code, cache_hit=False):
    performance_data['operation'].append(operation)
    performance_data['response_time'].append(response_time)
    performance_data['status_code'].append(status_code)
    performance_data['timestamp'].append(datetime.now())
    performance_data['cache_hit'].append(cache_hit)

def check_server_status():
    """Check if both servers are running"""
    try:
        print("\nAttempting to connect to Express server...")
        start_time = time.time()
        response = requests.get(f"{SERVER_URL}/test", timeout=2)
        elapsed_time = time.time() - start_time
        log_request('server_status_check', elapsed_time, response.status_code)
        print(f"Express server response: {response.status_code}")
        print("✓ Express server is running")
    except requests.exceptions.ConnectionError as e:
        print(f"✗ Express server connection error: {str(e)}")
        return False
    except requests.exceptions.RequestException as e:
        print(f"✗ Express server error: {str(e)}")
        return False

    try:
        print("\nAttempting to connect to proxy server...")
        start_time = time.time()
        response = requests.get(f"{PROXY_URL}/test", timeout=2)
        elapsed_time = time.time() - start_time
        log_request('proxy_status_check', elapsed_time, response.status_code)
        print(f"Proxy server response: {response.status_code}")
        print("✓ Proxy server is running")
    except requests.exceptions.RequestException as e:
        print("✗ Proxy server is NOT running on port 8080")
        return False

    return True

def test_large_request(num_requests=3):
    """Test large request multiple times to better demonstrate caching effect"""
    print("\nTesting large request (10MB) through proxy...")
    headers = {'Host': 'localhost:3000'}
    results = []
    
    for i in range(num_requests):
        print(f"Request {i+1}/{num_requests}...")
        start_time = time.time()
        response = requests.get(f"{PROXY_URL}/large", headers=headers)
        elapsed_time = time.time() - start_time
        
        cache_hit = i > 0  # First request is always a cache miss
        log_request(
            f'large_request_{"cached" if cache_hit else "nocache"}', 
            elapsed_time, 
            response.status_code,
            cache_hit
        )
        print(f"Request {i+1} time: {elapsed_time:.4f} seconds")
        # Add small delay between requests
        time.sleep(0.5)

def create_test_users(num_users=5):
    print(f"\nCreating {num_users} test users...")
    created_users = []
    
    for i in range(num_users):
        user_data = {
            "id": str(i + 1),
            "name": f"Test User {i + 1}",
            "email": f"user{i + 1}@example.com"
        }
        headers = {'Content-Type': 'application/json'}
        
        start_time = time.time()
        response = requests.post(
            f"{SERVER_URL}/users",
            json=user_data,
            headers=headers
        )
        elapsed_time = time.time() - start_time
        log_request('create_user', elapsed_time, response.status_code)
        
        if response.status_code == 201:
            created_users.append(response.json())
            print(f"Created user {i + 1}")
    
    return created_users

def get_users():
    print("\nRetrieving users through proxy...")
    headers = {'Host': 'localhost:3000'}
    
    start_time = time.time()
    response = requests.get(f"{PROXY_URL}/users", headers=headers)
    elapsed_time = time.time() - start_time
    
    log_request('get_users', elapsed_time, response.status_code)
    
    if response.status_code == 200:
        users = response.json()
        print(f"Retrieved {len(users)} users")
        return users
    else:
        print(f"Failed to retrieve users. Status code: {response.status_code}")
        return []

#def delete_users(user_ids):
#    print(f"\nDeleting {len(user_ids)} users...")
#    for user_id in user_ids:
#        start_time = time.time()
#        response = requests.delete(f"{SERVER_URL}/users/{user_id}")
#        elapsed_time = time.time() - start_time
#        log_request('delete_user', elapsed_time, response.status_code)
#        print(f"Deleted user {user_id}")

def generate_performance_graphs():
    """Generate improved performance visualization"""
    df = pd.DataFrame(performance_data)
    
    # Use a clean, built-in style
    plt.style.use('bmh')
    fig = plt.figure(figsize=(15, 10))
    
    # 1. Cache Performance Comparison
    plt.subplot(2, 1, 1)
    cache_data = df[df['operation'].str.contains('large_request')]
    
    # Calculate statistics
    stats = cache_data.groupby('operation')['response_time'].agg(['mean', 'std']).reset_index()
    x_pos = np.arange(len(stats['operation']))
    
    # Create bar plot
    bars = plt.bar(x_pos, stats['mean'], 
                  yerr=stats['std'],
                  color=['lightcoral', 'lightgreen'],
                  capsize=5)
    
    # Customize the plot
    plt.title('Cache Performance Impact on 10MB Request', fontsize=14, pad=20)
    plt.ylabel('Response Time (seconds)', fontsize=12)
    plt.xlabel('Request Type', fontsize=12)
    plt.xticks(x_pos, ['First Request', 'Cached Requests'], fontsize=10)
    
    # Add value labels on top of bars
    for bar in bars:
        height = bar.get_height()
        plt.text(bar.get_x() + bar.get_width()/2., height,
                f'{height:.2f}s',
                ha='center', va='bottom')
    
    # Calculate improvement percentage
    nocache_time = stats[stats['operation'] == 'large_request_nocache']['mean'].iloc[0]
    cache_time = stats[stats['operation'] == 'large_request_cached']['mean'].iloc[0]
    improvement = ((nocache_time - cache_time) / nocache_time) * 100
    
    plt.text(0.5, -0.15, 
             f'Cache Performance Improvement: {improvement:.1f}%',
             ha='center',
             transform=plt.gca().transAxes,
             fontsize=12,
             bbox=dict(facecolor='white', alpha=0.8, edgecolor='none', pad=5))

    # 2. Operation Response Times (excluding large requests)
    plt.subplot(2, 1, 2)
    regular_ops = df[~df['operation'].str.contains('large_request')]
    
    # Group data by operation
    grouped_data = []
    labels = []
    for op in regular_ops['operation'].unique():
        grouped_data.append(regular_ops[regular_ops['operation'] == op]['response_time'].values)
        labels.append(op.replace('_', ' ').title())
    
    # Create box plot
    plt.boxplot(grouped_data, labels=labels)
    plt.title('Response Times by Operation Type (Excluding Large Requests)', fontsize=14, pad=20)
    plt.xticks(rotation=45, ha='right')
    plt.ylabel('Response Time (seconds)', fontsize=12)
    
    # Adjust layout
    plt.tight_layout(pad=3.0)
    plt.savefig('performance_analysis.png', bbox_inches='tight', dpi=300)
    print("\nPerformance graphs saved as 'performance_analysis.png'")

def print_performance_summary():
    """Print improved performance summary with detailed statistics"""
    df = pd.DataFrame(performance_data)
    print("\nPerformance Summary:")
    print("-" * 50)
    
    # Calculate cache performance with more detail
    cache_data = df[df['operation'].str.contains('large_request')]
    nocache_stats = cache_data[cache_data['operation'] == 'large_request_nocache']['response_time']
    cache_stats = cache_data[cache_data['operation'] == 'large_request_cached']['response_time']
    
    print(f"\nLarge Request (10MB) Performance Analysis:")
    print(f"First request (no cache): {nocache_stats.mean():.4f} seconds")
    print(f"Cached requests:")
    print(f"  - Average: {cache_stats.mean():.4f} seconds")
    print(f"  - Min: {cache_stats.min():.4f} seconds")
    print(f"  - Max: {cache_stats.max():.4f} seconds")
    print(f"  - Standard deviation: {cache_stats.std():.4f} seconds")
    print(f"Cache speedup: {((nocache_stats.mean() - cache_stats.mean()) / nocache_stats.mean() * 100):.1f}%")
    
    print("\nOther Operations (mean response times):")
    regular_ops = df[~df['operation'].str.contains('large_request')]
    summary = regular_ops.groupby('operation')['response_time'].agg(['count', 'mean', 'min', 'max', 'std'])
    summary.columns = ['Count', 'Mean', 'Min', 'Max', 'Std Dev']
    print(summary.round(4))

if __name__ == "__main__":
    print("Starting enhanced test sequence...")
    
    # Check if servers are running
    if not check_server_status():
        sys.exit(1)
    
    # Test large request caching with multiple attempts
    test_large_request(3)
    
    # Create and delete multiple users
    created_users = create_test_users(5)
    users = get_users()
    delete_users([user['id'] for user in created_users])
    
    # Generate performance analysis
    generate_performance_graphs()
    print_performance_summary()
