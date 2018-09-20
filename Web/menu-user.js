document.write('<a href="index.html" class="logo"></a>');

document.write('\
<div class="swich"><span>HP OFF</span>\
    <div class="onoffswitch">\
        <input type="checkbox" name="onoffswitch" class="onoffswitch-checkbox" id="onoffswitch" onchange="swich(\'onoffswitch\')">\
        <label class="onoffswitch-label" for="onoffswitch"><span class="onoffswitch-inner"></span><span class="onoffswitch-switch"></span></label>\
    </div><span>HP ON</span></div>\
<div class="menu-profiles">\
	<span id="get_mode" style="color: red">-</span><br>\
ПРОФИЛЬ<br>\
    <select id="get_listprofile" onchange="setParam(\'get_listProfile\',\'get_listprofile\');"></select>\
</div>');

document.write('<ul>\
    <li class="index"><a href="index.html"><i></i>Состояние</a></li>\
    <li class="plan"><a href="plan.html"><i></i>Схема ТН</a></li>\
    <li class="about"><a href="about.html"><i></i>О контроллере</a></li>\
</ul>');

document.write('\
<div class="dateinfo">\
    <div id="get_status"></div>\
    <div>&#128241 <a href="/mob/">Мобильная версия</a></div>\
</div>');

var extract = new RegExp('[a-z0-9-]+\.html'); // /v039/index.html
var pathname = location.pathname;
pathmath = pathname.match(extract);
if(!pathmath) {var activeli = "index";} else {var activeli = pathmath.toString().replace(/\.html/g,"");}
var elements = document.getElementsByClassName(activeli);
var countElements = elements.length;
for(i=0;i<countElements;i++){document.getElementsByClassName(activeli)[i].classList.add("active");}
updateParam("get_status,get_WORK,get_MODE,get_listProfile");
