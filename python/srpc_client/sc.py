#! /usr/bin/python3

import http.client

conn = http.client.HTTPConnection("hq.sinajs.cn")
conn.request("GET", "/list=sh600019")
resp = conn.getresponse()
print ( resp.status )
data = resp.read()
print (data)
conn.close()

