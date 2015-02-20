# Curb - Libcurl bindings for Ruby

* [rubyforge rdoc](http://curb.rubyforge.org/)
* [rubyforge project](http://rubyforge.org/projects/curb)
* [github project](http://github.com/taf2/curb/tree/master)

Curb (probably CUrl-RuBy or something) provides Ruby-language bindings for the
libcurl(3), a fully-featured client-side URL transfer library.
cURL and libcurl live at [http://curl.haxx.se/](http://curl.haxx.se/) .

Curb is a work-in-progress, and currently only supports libcurl's 'easy' and 'multi' modes.

## License

Curb is copyright (c)2006 Ross Bamford, and released under the terms of the 
Ruby license. See the LICENSE file for the gory details. 

## You will need

* A working Ruby installation (1.8+, tested with 1.8.6, 1.8.7, 1.9.1, and 1.9.2)
* A working (lib)curl installation, with development stuff (7.5+, tested with 7.19.x)
* A sane build environment (e.g. gcc, make)

## Installation...

... will usually be as simple as:

    $ gem install curb

On Windows, make sure you're using the [DevKit](http://rubyinstaller.org/downloads/) and
the [development version of libcurl](http://curl.haxx.se/gknw.net/7.39.0/dist-w32/curl-7.39.0-devel-mingw32.zip). Unzip, then run this in your command
line (alter paths to your curl location, but remember to use forward slashes):

    gem install curb --platform=ruby -- --with-curl-lib=C:/curl-7.39.0-devel-mingw32/bin --with-curl-include=C:/curl-7.39.0-devel-mingw32/include

Or, if you downloaded the archive:  

    $ rake install 

If you have a weird setup, you might need extconf options. In this case, pass
them like so:

    $ rake install EXTCONF_OPTS='--with-curl-dir=/path/to/libcurl --prefix=/what/ever'
  
Curb is tested only on GNU/Linux x86 and Mac OSX - YMMV on other platforms.
If you do use another platform and experience problems, or if you can 
expand on the above instructions, please report the issue at http://github.com/taf2/curb/issues

On Ubuntu, the dependencies can be satisfied by installing the following packages:

    $ sudo apt-get install libcurl3 libcurl3-gnutls libcurl4-openssl-dev

On RedHat:

    $ sudo yum install ruby-devel libcurl-devel openssl-devel
    
Curb has fairly extensive RDoc comments in the source. You can build the
documentation with:

    $ rake doc

## Usage & examples

Curb provides two classes:

* `Curl::Easy` - simple API, for day-to-day tasks.
* `Curl::Multi` - more advanced API, for operating on multiple URLs simultaneously.

### Super simple API (less typing)

```ruby
http = Curl.get("http://www.google.com/")
puts http.body_str

http = Curl.post("http://www.google.com/", {:foo => "bar"})
puts http.body_str

http = Curl.get("http://www.google.com/") do|http|
  http.headers['Cookie'] = 'foo=1;bar=2'
end
puts http.body_str
```

### Simple fetch via HTTP:

```ruby
c = Curl::Easy.perform("http://www.google.co.uk")
puts c.body_str
```

Same thing, more manual:

```ruby
c = Curl::Easy.new("http://www.google.co.uk")
c.perform
puts c.body_str
```

### Additional config:

```ruby
Curl::Easy.perform("http://www.google.co.uk") do |curl| 
  curl.headers["User-Agent"] = "myapp-0.0"
  curl.verbose = true
end
```

Same thing, more manual:

```ruby
c = Curl::Easy.new("http://www.google.co.uk") do |curl| 
  curl.headers["User-Agent"] = "myapp-0.0"
  curl.verbose = true
end

c.perform
```

### HTTP basic authentication:

```ruby
c = Curl::Easy.new("http://github.com/")
c.http_auth_types = :basic
c.username = 'foo'
c.password = 'bar'
c.perform
```

### HTTP "insecure" SSL connections (like curl -k, --insecure) to avoid Curl::Err::SSLCACertificateError:

```ruby
    c = Curl::Easy.new("http://github.com/")
    c.ssl_verify_peer = false
    c.perform
```

### Supplying custom handlers:

```ruby
c = Curl::Easy.new("http://www.google.co.uk")

c.on_body { |data| print(data) }
c.on_header { |data| print(data) }

c.perform
```

### Reusing Curls:

```ruby
c = Curl::Easy.new

["http://www.google.co.uk", "http://www.ruby-lang.org/"].map do |url|
  c.url = url
  c.perform
  c.body_str
end
```

### HTTP POST form:

```ruby
c = Curl::Easy.http_post("http://my.rails.box/thing/create",
                         Curl::PostField.content('thing[name]', 'box'),
                         Curl::PostField.content('thing[type]', 'storage'))
```

### HTTP POST file upload:

```ruby
c = Curl::Easy.new("http://my.rails.box/files/upload")
c.multipart_form_post = true
c.http_post(Curl::PostField.file('thing[file]', 'myfile.rb'))
```

### Multi Interface (Basic HTTP GET):

```ruby
# make multiple GET requests
easy_options = {:follow_location => true}
multi_options = {:pipeline => true}

Curl::Multi.get('url1','url2','url3','url4','url5', easy_options, multi_options) do|easy|
  # do something interesting with the easy response
  puts easy.last_effective_url
end
```

### Multi Interface (Basic HTTP POST):

```ruby
# make multiple POST requests
easy_options = {:follow_location => true, :multipart_form_post => true}
multi_options = {:pipeline => true}

url_fields = [
  { :url => 'url1', :post_fields => {'f1' => 'v1'} },
  { :url => 'url2', :post_fields => {'f1' => 'v1'} },
  { :url => 'url3', :post_fields => {'f1' => 'v1'} }
]

Curl::Multi.post(url_fields, easy_options, multi_options) do|easy|
  # do something interesting with the easy response
  puts easy.last_effective_url
end
```

### Multi Interface (Advanced):

```ruby
responses = {}
requests = ["http://www.google.co.uk/", "http://www.ruby-lang.org/"]
m = Curl::Multi.new
# add a few easy handles
requests.each do |url|
  responses[url] = ""
  c = Curl::Easy.new(url) do|curl|
    curl.follow_location = true
    curl.on_body{|data| responses[url] << data; data.size }
    curl.on_success {|easy| puts "success, add more easy handles" }
  end
  m.add(c)
end

m.perform do
  puts "idling... can do some work here"
end

requests.each do|url|
  puts responses[url]
end
```

### Easy Callbacks

* `on_success`  is called when the response code is 2xx
* `on_redirect` is called when the response code is 3xx
* `on_missing` is called when the response code is 4xx
* `on_failure` is called when the response code is 5xx
* `on_complete` is called in all cases.
