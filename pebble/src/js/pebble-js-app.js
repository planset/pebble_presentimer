var ws;
var configUrl = "http://dkpyn.com/sandbox/pebble/config.html";

Pebble.addEventListener("ready",
    function(e) {
        console.log("ready");
    }
);

function controlIdToUrl(id) {
    if (id == 1) {
        return "next";
    } else {
        return "back";
    }
}

Pebble.addEventListener("appmessage", function(e) {
    console.log(e.payload.control);

    var host = localStorage.getItem("host");
    var url = "http://" + host + "/" + controlIdToUrl(e.payload.control);

    var req = new XMLHttpRequest();
    req.open('GET', url);
    req.send();

});

Pebble.addEventListener("showConfiguration", function() {
    console.log("showing configuration");
    var options = {host: localStorage.getItem('host')};
    var encodedOptions = encodeURIComponent(JSON.stringify(options));
    Pebble.openURL(configUrl + '#' + encodedOptions);
});

Pebble.addEventListener("webviewclosed", function(e) {
    console.log("configuration closed");
    options = JSON.parse(decodeURIComponent(e.response));
    localStorage.setItem('host', options.host);
    console.log("Options = " + JSON.stringify(options));
});;
