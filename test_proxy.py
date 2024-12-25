import requests
import time
import statistics

def test_proxy_cache(url, num_requests=5):
    """
    Test proxy server performance and caching
    
    Args:
        url: The URL to test with
        num_requests: Number of repeated requests to make
    """
    proxy = {
        'http': 'http://localhost:8080',
        'https': 'http://localhost:8080'
    }
    
    # Lists to store response times
    first_request_times = []
    cached_request_times = []
    
    print(f"Testing proxy with URL: {url}")
    print(f"Making {num_requests} requests...\n")
    
    for i in range(num_requests):
        # Clear session to ensure fresh connection
        session = requests.Session()
        
        # First request (potentially uncached)
        start_time = time.time()
        response = session.get(url, proxies=proxy)
        end_time = time.time()
        first_time = end_time - start_time
        first_request_times.append(first_time)
        
        print(f"Request {i+1}:")
        print(f"  First request: {first_time:.3f} seconds")
        
        # Immediate second request (should be cached)
        start_time = time.time()
        response = session.get(url, proxies=proxy)
        end_time = time.time()
        cached_time = end_time - start_time
        cached_request_times.append(cached_time)
        
        print(f"  Cached request: {cached_time:.3f} seconds")
        print(f"  Speed improvement: {((first_time - cached_time) / first_time * 100):.1f}%")
        print()
        
        # Wait a bit between test cycles
        time.sleep(1)
    
    # Print summary statistics
    print("\nTest Summary:")
    print(f"Average first request time: {statistics.mean(first_request_times):.3f} seconds")
    print(f"Average cached request time: {statistics.mean(cached_request_times):.3f} seconds")
    print(f"Average speed improvement: {((statistics.mean(first_request_times) - statistics.mean(cached_request_times)) / statistics.mean(first_request_times) * 100):.1f}%")

if __name__ == "__main__":
    # Test with a few different URLs
    test_urls = [
        'http://example.com',
        'http://httpbin.org/get',
        'http://httpbin.org/headers'
    ]
    
    for url in test_urls:
        test_proxy_cache(url)
        print("\n" + "="*50 + "\n")
