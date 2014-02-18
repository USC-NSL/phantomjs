# [PhantomJS](http://phantomjs.org) - Scriptable Headless WebKit

PhantomJS ([www.phantomjs.org](http://phantomjs.org)) is a headless WebKit scriptable with JavaScript or CoffeeScript. It is used by hundreds of [developers](https://github.com/ariya/phantomjs/wiki/Buzz) and dozens of [organizations](https://github.com/ariya/phantomjs/wiki/Users) for web-related development workflow.

This fork of PhantomJS has modifications to give the invoker more control over DNS lookups. 

Usage: phantomjs --dns=/home/user/dns.cfg hello.js http://www.google.com

dns.cfg file format:
```
google.com = 74.125.224.246,ssl.gstatic.com,
ssl.gstatic.com = 74.125.224.80,gstatic.com,
```

On the right hand side, the ip is the DNS entry for the hostname. The hostname to right sets the HTTP "Host:" header.

Chages:
- networkaccessmanager.cpp
