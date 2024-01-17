(function() {
    console.log("script.js");
    var f = document.getElementById('f');
    
    if (f.files.length)
      processFile();

    var btn = document.getElementById('btn');
    
    if (f.files.length)
      processFile();
    
    // f.addEventListener('change', processFile, false);
    btn.addEventListener('click', processFile, false);
    
    
    function processFile(e) {
        console.log("processFile");
      var file = f.files[0];
      var size = file.size;
      var sliceSize = 256;
      var start = 0;
    
      setTimeout(loop, 1);
    
      function loop() {
        var end = start + sliceSize;
        
        if (size - end < 0) {
          end = size;
        }
        
        var s = slice(file, start, end);
    
        send(s, start, end);
    
        if (end < size) {
          start += sliceSize;
          setTimeout(loop, 1);
        }
      }
    }
    
    
    function send(piece, start, end) {
      var formdata = new FormData();
      var xhr = new XMLHttpRequest();
    
      xhr.open('POST', '/chunks', true);
    
      formdata.append('start', start);
      formdata.append('end', end);
      formdata.append('file', piece);
    
      xhr.send(formdata);
    }
    
    /**
     * Formalize file.slice
     */
    
    function slice(file, start, end) {
      var slice = file.mozSlice ? file.mozSlice :
                  file.webkitSlice ? file.webkitSlice :
                  file.slice ? file.slice : noop;
      
      return slice.bind(file)(start, end);
    }
    
    function noop() {
      
    }
    
})();