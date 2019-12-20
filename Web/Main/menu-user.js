document.write('<a href="index.html" class="logo"></a>');
document.write('\
<div class="swich"><span>HP OFF</span>\
<div class="onoffswitch">\
	<input type="checkbox" name="onoffswitch" class="onoffswitch-checkbox" id="onoffswitch" onchange="swich(\'onoffswitch\')">\
	<label class="onoffswitch-label" for="onoffswitch"><span class="onoffswitch-inner"></span><span class="onoffswitch-switch"></span></label>\
</div><span>HP ON</span></div>\
<div class="menu-profiles">\
	<span id="get_mode" style="color: red">-</span><br>\
	<span id="SCH-PR"></span><br>\
	<select id="get_listprofile" onchange="setParam(\'get_listProfile\',\'get_listprofile\');"></select>\
</div>');
document.write('<ul class="cd-accordion-menu">\
<li class="index"><a href="index.html"><i></i>Состояние</a></li>\
<li class="plan"><a href="plan.html"><i></i>Схема ТН</a></li>\
<li class="stats history has-children">\
	<input type="checkbox" name="group-2" id="group-2">\
	<label for="group-2"><i></i>Статистика</label>\
	<ul>\
		<li class="stats"><a href="stats.html">По дням</a></li>\
		<li class="history"><a href="history.html">Детально</a></li>\
	</ul>\
</li>\
<li class="about"><a href="about.html"><i></i>О контроллере</a></li>\
<li class="login"><a href="lan.html" onclick="NeedLogin=1"><i></i>Логин</a></li>\
</ul>');
document.write('\
<div class="dateinfo">\
	<div id="get_status"></div>\
</div>');
// <div>&#128241 <a href="/mob/">Мобильная версия</a></div>\
var extract = new RegExp('[a-z0-9-]+\.html');
var pathname = location.pathname;
pathmath = pathname.match(extract);
if(!pathmath) {var activeli = document.body.className;} else {var activeli = pathmath.toString().replace(/(-set.*)?\.html/g,"");}
var elements = document.getElementsByClassName(activeli);
var countElements = elements.length;
for(i=0;i<countElements;i++){document.getElementsByClassName(activeli)[i].classList.add("active");}
updateParam("get_status,get_WORK,get_MODE,get_listProfile,get_SCHDLR(On)");
