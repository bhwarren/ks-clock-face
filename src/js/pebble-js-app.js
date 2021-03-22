Pebble.addEventListener('showConfiguration', function(e) {
  // Show config page
  Pebble.openURL('http://bhwarren.github.io/ks-clock-face/ks-clock-color.html');
});


Pebble.addEventListener('webviewclosed', function(e) {
  var color = parseInt(e.response.split("=")[1]);
  console.log(color);
  
  Pebble.sendAppMessage({ COLOR_KEY: color }, 
    function(e) {
      console.log('Successfully delivered message with transactionId=' + e.data.transactionId);
    }, 
    function(e) {
      console.log('Unable to deliver message with transactionId=' + e.data.transactionId + ' Error is: ' + e.error.message);
    });
});