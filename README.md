Here’s an improved and well-organized version of the instructions suitable for a GitHub README:

---

# Multithreaded Proxy Server with Caching

This repository contains a multithreaded proxy server written in C with caching functionality to improve performance by reducing the need to repeatedly fetch the same resources. Below are the steps to set up, run, and test the proxy server.

## Prerequisites

- Ubuntu or any Linux-based system
- `gcc` (GNU Compiler Collection) installed
- `curl` for testing HTTP requests
- Python 3 and `requests` library for cache testing and analytics

## Setup and Running the Proxy Server

1. **Copy the `proxy_server.c` File:**

   The `proxy_server.c` file is included in the repository by default.

2. **Compile the Proxy Server:**

   Use `gcc` to compile the server with pthread support (for multithreading):

   ```bash
   gcc -o proxy_server proxy_server.c -pthread
   ```

3. **Run the Proxy Server:**

   To start the proxy server on a specific port (e.g., `8080`):

   ```bash
   ./proxy_server 8080
   ```

   The server will start listening on the specified port (in this case, `8080`).

   **Example Output:**

   ```bash
   Proxy server listening on port 8080...
   ```

## Testing the Proxy Server

### Test with `curl`

To verify that the proxy server is working:

1. **Open a New Terminal Window** and run the following `curl` commands:

   - **First request:**

     ```bash
     curl --proxy http://localhost:8080 http://example.com
     ```

   - **Second request (to test caching):**

     ```bash
     curl --proxy http://localhost:8080 http://example.com
     ```

   **Expected Output:**

   The first request will fetch the resource, while the second request will serve the cached content.

   ```bash
   <html>
   <head>
       <title>Example Domain</title>
       ...
   </head>
   <body>
       ...
   </body>
   </html>
   <html>
   <head>
       <title>Example Domain</title>
       ...
   </head>
   <body>
       ...
   </body>
   </html>
   ```

   On the server side, you should see log messages like:

   ```bash
   New connection from 127.0.0.1:54706
   New connection from 127.0.0.1:54708
   Cache hit - serving from cache
   ```

### Cache Test and Analytics with Python Script

To perform cache performance testing and analytics:

1. **Install Python Dependencies:**

   First, install the `requests` library:

   ```bash
   pip install requests
   ```

2. **Run the Cache Test Script:**

   Execute the `test_proxy.py` script to test the proxy server’s caching mechanism:

   ```bash
   python test_proxy.py
   ```

   **Sample Output:**

   The script will test the proxy server with multiple requests to various URLs and display caching performance, including the speed improvement between first-time requests and cached requests.

   **Example Output:**

   ```bash
   Testing proxy with URL: http://example.com
   Making 5 requests...

   Request 1:
     First request: 0.455 seconds
     Cached request: 0.011 seconds
     Speed improvement: 97.7%

   Request 2:
     First request: 0.003 seconds
     Cached request: 0.003 seconds
     Speed improvement: 5.2%

   Request 3:
     First request: 0.008 seconds
     Cached request: 0.008 seconds
     Speed improvement: -3.0%

   Request 4:
     First request: 0.007 seconds
     Cached request: 0.038 seconds
     Speed improvement: -406.5%

   Request 5:
     First request: 0.003 seconds
     Cached request: 0.335 seconds
     Speed improvement: -11193.5%

   Test Summary:
   Average first request time: 0.095 seconds
   Average cached request time: 0.079 seconds
   Average speed improvement: 17.2%
   ```

   The script also performs similar tests for other URLs like `httpbin.org/get` and `httpbin.org/headers`.

---

### Key Features:

- **Multithreaded**: Handles multiple connections concurrently using pthreads.
- **Caching**: Improves performance by serving cached responses for previously requested resources.
- **Testing**: Includes a Python script to test cache effectiveness and speed improvements.

## Contribution

Feel free to fork the repository, contribute with pull requests, or report any issues you encounter.

---

This README is now well-organized, easy to follow, and includes detailed instructions on setting up, running, and testing the proxy server. It also provides sample outputs to help users understand the expected behavior during the setup and testing phases.
