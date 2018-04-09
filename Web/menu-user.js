document.write('<a href="index.html" class="logo"></a>');

document.write('
<div class="swich"><span>ТН OFF</span>\
    <div class="onoffswitch">\
        <input type="checkbox" name="onoffswitch" class="onoffswitch-checkbox" id="onoffswitch" onchange="swich(\'onoffswitch\')">\
        <label class="onoffswitch-label" for="onoffswitch"><span class="onoffswitch-inner"></span><span class="onoffswitch-switch"></span></label>\
    </div><span>ТН ON</span></div>');

document.write('<ul>\
    <li class="index"><a href="index.html"><i></i>Состояние</a></li>\
    <li class="plan"><a href="plan.html"><i></i>Схема ТН</a></li>\
    <li class="about"><a href="about.html"><i></i>О контроллере</a></li>\
</ul>');

document.write('<div class="dateinfo">\
    <h1 id="get_datetime-time3">-</h1>\
    <h2 id="get_datetime-date3">-</h2>\
    <div class="ver"><span>Версия ПО:</span><span id="get_version">-</span><span>FREE RAM:</span><span id="get_freeram">-</span><span>CPU LOAD:</span><span id="get_loadingcpu">-</span><span>Uptime:</span><span id="get_uptime2">-</span><span>Режим:</span><span id="get_testmode2">-</span><span>Перегрев:</span><span id="get_overheateev2">-</span></div>\
</div>');

//var activeli = document.getElementsByTagName("body")[0].className;
var extract = new RegExp('[a-z0-9-]+\.html'); // /v039/index.html
var pathname = location.pathname;
pathmath = pathname.match(extract);
if(!pathmath) {var activeli = "index";} else {var activeli = pathmath.toString().replace(/\.html/g,"");}
var elements = document.getElementsByClassName(activeli);
var countElements = elements.length;
for(i=0;i<countElements;i++){document.getElementsByClassName(activeli)[i].classList.add("active");}
updateParam("get_version,get_testMode,get_datetime(TIME),get_datetime(DATE),get_WORK,get_uptime,get_freeRam,get_loadingCPU,get_uptime,get_overheatEEV");
