This plug-in tries to immitate the behaviour of a modern browser: it not only downloads an HTML document (which is already possible with the [filetransfer plug-in](https://github.com/shadow/shadow/wiki/Using-the-filetransfer-plug-in)), but also parses the HTML and downloads the following types of embedded objects:

+ favorite icons
+ stylesheets (embedded with a link-tag)
+ images (embedded with an img-tag)

## Example Usage

To simulate browser requests to www.wikipedia.org the site has to be mirrored to the local filesystem:

```bash
mkdir some-directory
cd some-directory
wget -H -p http://www.wikipedia.org
cd ../
```

This will create directories for each hostname (namely `bits.wikimedia.org`,`en.wikipedia.org`, `upload.wikimedia.org` and `www.wikipedia.org`) and download the HTML document and the embedded resources to the respective directory.

Then each hostname needs to have a node in Shadow. The host file, hosts.xml, for this example should therefore look something like this:

```xml
<!-- 
  -- plugin paths are relative to "~/.shadow/lib/" unless abosolute paths are given
  -->
<plugin id="filex" path="libshadow-plugin-filetransfer.so" />
<plugin id="browser" path="libshadow-plugin-browser.so" />
 
<!-- Wikipedia -->
<software id="wikipedia1-server" plugin="filex" time="10" arguments="server 80 some-directory/www.wikipedia.org/" />
<software id="wikipedia2-server" plugin="filex" time="10" arguments="server 80 some-directory/en.wikipedia.org/" />
<software id="wikipedia-cdn1-server" plugin="filex" time="10" arguments="server 80 some-directory/upload.wikimedia.org/" />
<software id="wikipedia-cdn2-server" plugin="filex" time="10" arguments="server 80 some-directory/bits.wikimedia.org/" />

<node id="www.wikipedia.org" software="wikipedia1-server" cluster="USMN" bandwidthdown="60000" bandwidthup="30000" cpufrequency="2800000" />
<node id="en.wikipedia.org" software="wikipedia2-server" cluster="USMN" bandwidthdown="60000" bandwidthup="30000" cpufrequency="2800000" />
<node id="upload.wikimedia.org" software="wikipedia-cdn1-server" cluster="USMN" bandwidthdown="60000" bandwidthup="30000" cpufrequency="2800000" />
<node id="bits.wikimedia.org" software="wikipedia-cdn2-server" cluster="USMN" bandwidthdown="60000" bandwidthup="30000" cpufrequency="2800000" />

<!-- Browser -->
<software id="client" plugin="browser" time="20" arguments="www.wikipedia.org 80 none 0 6 /index.html" />
<node id="client.node" software="client" quantity="1" />

<kill time="600" />
```

The arguments for the browser plugin denote the following:

1. HTTP server address/name
2. HTTP server port
3. SOCKS proxy address/name
4. SOCKS proxy port
5. Maximum amount of concurrent connections per host
6. Path of the HTML document

To run the example, make sure you have `./hosts.xml`, `./some-directory/`, and `~/.shadow/share/topology.xml`.

```
shadow ~/.shadow/share/topology.xml hosts.xml | grep browser_
```

The output should be like the following:

```
[browser_launch] Trying to simulate browser access to /index.html on www.wikipedia.org
[browser_downloaded_document] first document (46166 bytes) downloaded and parsed in 1.176 seconds, now getting 16 additional objects...
[browser_free] Finished downloading 16/16 embedded objects (28376 bytes) in 1.395 seconds
```

## Implementation

Like a browser, the plugin opens multiple persistent HTTP connections per host. The maximum of concurrent connections per host can be limited though (which all modern browser do [as well](http://www.browserscope.org/?category=network)). When there are more downloads than connections available the connections are reused once a download finishes.