Run this on Ubuntu/ any linux distro

copy the proxy_server.c file into the folder (done by def)

```bash
gcc -o proxy_server proxy_server.c -pthread

./proxy_server 8080  # or any other port number

```

will start the proxy server on port 8080

```bash
mihir@MihirSDudekula:~/mtps$ vim proxy_server.c
mihir@MihirSDudekula:~/mtps$ gcc -o proxy_server proxy_server.c -pthread
mihir@MihirSDudekula:~/mtps$ ./proxy_server 8080  # or any other port number
Proxy server listening on port 8080...
```

to check its working
open a new terminal window and run 

```bash
# Test with a simple HTTP request
curl --proxy http://localhost:8080 http://example.com

# Make the same request again to test caching
curl --proxy http://localhost:8080 http://example.com
```

we get the following response
```bash
mihir@MihirSDudekula:~$ # Test with a simple HTTP request
curl --proxy http://localhost:8080 http://example.com

# Make the same request again to test caching
curl --proxy http://localhost:8080 http://example.com
<!doctype html>
<html>
<head>
    <title>Example Domain</title>

    <meta charset="utf-8" />
    <meta http-equiv="Content-type" content="text/html; charset=utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <style type="text/css">
    body {
        background-color: #f0f0f2;
        margin: 0;
        padding: 0;
        font-family: -apple-system, system-ui, BlinkMacSystemFont, "Segoe UI", "Open Sans", "Helvetica Neue", Helvetica, Arial, sans-serif;

    }
    div {
        width: 600px;
        margin: 5em auto;
        padding: 2em;
        background-color: #fdfdff;
        border-radius: 0.5em;
        box-shadow: 2px 3px 7px 2px rgba(0,0,0,0.02);
    }
    a:link, a:visited {
        color: #38488f;
        text-decoration: none;
    }
    @media (max-width: 700px) {
        div {
            margin: 0 auto;
            width: auto;
        }
    }
    </style>
</head>

<body>
<div>
    <h1>Example Domain</h1>
    <p>This domain is for use in illustrative examples in documents. You may use this
    domain in literature without prior coordination or asking for permission.</p>
    <p><a href="https://www.iana.org/domains/example">More information...</a></p>
</div>
</body>
</html>
<!doctype html>
<html>
<head>
    <title>Example Domain</title>

    <meta charset="utf-8" />
    <meta http-equiv="Content-type" content="text/html; charset=utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <style type="text/css">
    body {
        background-color: #f0f0f2;
        margin: 0;
        padding: 0;
        font-family: -apple-system, system-ui, BlinkMacSystemFont, "Segoe UI", "Open Sans", "Helvetica Neue", Helvetica, Arial, sans-serif;

    }
    div {
        width: 600px;
        margin: 5em auto;
        padding: 2em;
        background-color: #fdfdff;
        border-radius: 0.5em;
        box-shadow: 2px 3px 7px 2px rgba(0,0,0,0.02);
    }
    a:link, a:visited {
        color: #38488f;
        text-decoration: none;
    }
    @media (max-width: 700px) {
        div {
            margin: 0 auto;
            width: auto;
        }
    }
    </style>
</head>

<body>
<div>
    <h1>Example Domain</h1>
    <p>This domain is for use in illustrative examples in documents. You may use this
    domain in literature without prior coordination or asking for permission.</p>
    <p><a href="https://www.iana.org/domains/example">More information...</a></p>
</div>
</body>
</html>
```

and on the server, we see

```bash

New connection from 127.0.0.1:54706
New connection from 127.0.0.1:54708
Cache hit - serving from cache
```

python script for cache test and analytics

```bash
pip install requests

python test_proxy.py

mihir@MihirSDudekula:~/mtps$ python3 test_proxy.py
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

==================================================

Testing proxy with URL: http://httpbin.org/get
Making 5 requests...

Request 1:
  First request: 0.462 seconds
  Cached request: 0.007 seconds
  Speed improvement: 98.5%

Request 2:
  First request: 0.007 seconds
  Cached request: 0.007 seconds
  Speed improvement: -6.8%

Request 3:
  First request: 0.007 seconds
  Cached request: 0.007 seconds
  Speed improvement: 1.9%

Request 4:
  First request: 0.007 seconds
  Cached request: 0.016 seconds
  Speed improvement: -145.7%

Request 5:
  First request: 0.007 seconds
  Cached request: 0.008 seconds
  Speed improvement: -10.4%


Test Summary:
Average first request time: 0.098 seconds
Average cached request time: 0.009 seconds
Average speed improvement: 90.9%

==================================================

Testing proxy with URL: http://httpbin.org/headers
Making 5 requests...

Request 1:
  First request: 0.483 seconds
  Cached request: 0.007 seconds
  Speed improvement: 98.5%

Request 2:
  First request: 0.006 seconds
  Cached request: 0.007 seconds
  Speed improvement: -15.5%

Request 3:
  First request: 0.008 seconds
  Cached request: 0.010 seconds
  Speed improvement: -34.2%

Request 4:
  First request: 0.007 seconds
  Cached request: 0.007 seconds
  Speed improvement: 2.8%

Request 5:
  First request: 0.007 seconds
  Cached request: 0.007 seconds
  Speed improvement: 3.7%


Test Summary:
Average first request time: 0.102 seconds
Average cached request time: 0.008 seconds
Average speed improvement: 92.4%

==================================================
```