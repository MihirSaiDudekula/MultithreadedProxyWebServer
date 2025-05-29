'''
Test Suite for Multithreaded Proxy Server

This script performs comprehensive testing of the proxy server and backend server,
measuring performance metrics and validating functionality.
'''

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
import socket

# Server configuration constants
'''
Server URLs:
- SERVER_URL: URL of the backend Express server
- PROXY_URL: URL of the caching proxy server
'''
SERVER_URL = "http://localhost:3000"
PROXY_URL = "http://localhost:8080"

# Performance tracking data structure
'''
performance_data: Dictionary to store performance metrics
- operation: Type of operation being performed
- response_time: Time taken for the operation
- status_code: HTTP status code returned
- timestamp: When the operation was performed
- cache_hit: Whether the request was served from cache
'''
performance_data = {
    'operation': [],
    'response_time': [],
    'status_code': [],
    'timestamp': [],
    'cache_hit': []
}

def get_local_ip():
    """
    Get local IP address in a cross-platform way
    
    Returns:
        str: Local IP address or 'localhost' if unable to determine
    """
    try:
        # Create a socket to connect to Google's DNS server
        # This is a reliable way to get the local IP address
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except:
        # Fallback to localhost if unable to get IP
        return "localhost"

def log_request(operation, response_time, status_code, cache_hit=False):
    performance_data['operation'].append(operation)
    performance_data['response_time'].append(response_time)
    performance_data['status_code'].append(status_code)
    performance_data['timestamp'].append(datetime.now())
    performance_data['cache_hit'].append(cache_hit)

def check_server_status():
    """Check if both servers are running with detailed error handling"""
    try:
        print("\nTesting Express server connectivity...")
        start_time = time.time()
        response = requests.get(f"{SERVER_URL}/test", timeout=2)
        elapsed_time = time.time() - start_time
        log_request('server_status_check', elapsed_time, response.status_code)
        
        if response.status_code == 200:
            print(f"✓ Express server is running (Response: {response.text})")
        else:
            print(f"✗ Express server returned unexpected status: {response.status_code}")
            return False
    except requests.exceptions.ConnectionError:
        print("✗ Cannot connect to Express server")
        return False
    except requests.exceptions.Timeout:
        print("✗ Express server connection timed out")
        return False
    except requests.exceptions.RequestException as e:
        print(f"✗ Express server error: {str(e)}")
        return False

    try:
        print("\nTesting proxy server connectivity...")
        start_time = time.time()
        response = requests.get(f"{PROXY_URL}/test", timeout=2, headers={'Host': 'localhost:3000'})
        elapsed_time = time.time() - start_time
        log_request('proxy_status_check', elapsed_time, response.status_code)
        
        if response.status_code == 200:
            print(f"✓ Proxy server is running (Response: {response.text})")
        else:
            print(f"✗ Proxy server returned unexpected status: {response.status_code}")
            return False
    except requests.exceptions.ConnectionError:
        print("✗ Cannot connect to proxy server")
        return False
    except requests.exceptions.Timeout:
        print("✗ Proxy server connection timed out")
        return False
    except requests.exceptions.RequestException as e:
        print(f"✗ Proxy server error: {str(e)}")
        return False

    return True

def test_large_request(num_requests=3, size_mb=10):
    """Test large request handling and caching performance"""
    print(f"\nTesting large request ({size_mb}MB) through proxy...")
    headers = {'Host': 'localhost:3000'}
    results = []
    
    for i in range(num_requests):
        print(f"Request {i+1}/{num_requests}...")
        start_time = time.time()
        try:
            response = requests.get(f"{PROXY_URL}/large", headers=headers, timeout=10)
            elapsed_time = time.time() - start_time
            
            if response.status_code != 200:
                print(f"✗ Request {i+1} failed with status {response.status_code}")
                continue
            
            cache_hit = i > 0  # First request is always a cache miss
            log_request(
                f'large_request_{"cached" if cache_hit else "nocache"}', 
                elapsed_time, 
                response.status_code,
                cache_hit
            )
            
            print(f"Request {i+1} time: {elapsed_time:.4f} seconds")
            print(f"Response size: {len(response.content) / (1024 * 1024):.2f} MB")
            
            results.append({
                'request_number': i + 1,
                'time': elapsed_time,
                'cache_hit': cache_hit,
                'status_code': response.status_code
            })
            
            # Add small delay between requests
            time.sleep(0.5)
        except requests.exceptions.RequestException as e:
            print(f"✗ Request {i+1} failed: {str(e)}")
            continue
    
    return results

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
    """Generate comprehensive performance visualization"""
    df = pd.DataFrame(performance_data)
    
    # Use a clean, built-in style
    plt.style.use('bmh')
    
    # Create multiple subplots
    fig, axes = plt.subplots(3, 2, figsize=(18, 15))
    fig.suptitle('Proxy Server Performance Analysis', fontsize=16)
    
    # 1. Cache Performance Comparison
    cache_data = df[df['operation'].str.contains('large_request')]
    
    # Calculate statistics
    stats = cache_data.groupby('operation')['response_time'].agg(['mean', 'std']).reset_index()
    
    # Bar plot for cache performance
    ax = axes[0, 0]
    bars = ax.bar(
        ['First Request', 'Cached Requests'],
        stats['mean'],
        yerr=stats['std'],
        color=['lightcoral', 'lightgreen'],
        capsize=5
    )
    
    ax.set_title('Cache Performance Impact')
    ax.set_ylabel('Response Time (seconds)')
    
    # Add value labels
    for bar in bars:
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
                f'{height:.2f}s',
                ha='center', va='bottom')
    
    # Calculate improvement
    nocache_time = stats[stats['operation'] == 'large_request_nocache']['mean'].iloc[0]
    cache_time = stats[stats['operation'] == 'large_request_cached']['mean'].iloc[0]
    improvement = ((nocache_time - cache_time) / nocache_time) * 100
    
    ax.text(0.5, -0.15, 
             f'Cache Performance Improvement: {improvement:.1f}%',
             ha='center',
             transform=ax.transAxes,
             fontsize=12,
             bbox=dict(facecolor='white', alpha=0.8, edgecolor='none', pad=5))

    # 2. Response Time Distribution
    ax = axes[0, 1]
    sns.boxplot(data=df, x='operation', y='response_time', ax=ax)
    ax.set_title('Response Time Distribution')
    ax.set_ylabel('Response Time (seconds)')
    ax.set_xticklabels(ax.get_xticklabels(), rotation=45, ha='right')

    # 3. Cache Hit Rate
    ax = axes[1, 0]
    cache_stats = df.groupby('cache_hit').size()
    cache_stats.plot(kind='pie', autopct='%.1f%%', ax=ax)
    ax.set_title('Cache Hit Rate')
    ax.set_ylabel('')

    # 4. Status Code Distribution
    ax = axes[1, 1]
    status_stats = df['status_code'].value_counts()
    status_stats.plot(kind='bar', ax=ax)
    ax.set_title('Status Code Distribution')
    ax.set_ylabel('Count')

    # 5. Request Timeline
    ax = axes[2, 0]
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    df.set_index('timestamp', inplace=True)
    df['response_time'].plot(ax=ax)
    ax.set_title('Request Response Times Over Time')
    ax.set_ylabel('Response Time (seconds)')

    # 6. Error Rate
    ax = axes[2, 1]
    error_rate = df[df['status_code'] >= 400].groupby('operation').size() / df.groupby('operation').size() * 100
    error_rate.plot(kind='bar', ax=ax)
    ax.set_title('Error Rate by Operation')
    ax.set_ylabel('Error Rate (%)')
    
    plt.tight_layout(rect=[0, 0, 1, 0.95])
    plt.savefig('performance_analysis.png')
    print("\nPerformance analysis saved to 'performance_analysis.png'")
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
