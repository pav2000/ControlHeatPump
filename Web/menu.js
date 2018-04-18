document.write('<a href="index.html" class="logo"></a>');

document.write('\
<div class="swich"><span>ТН OFF</span>\
    <div class="onoffswitch">\
        <input type="checkbox" name="onoffswitch" class="onoffswitch-checkbox" id="onoffswitch" onchange="swich(\'onoffswitch\')">\
        <label class="onoffswitch-label" for="onoffswitch"><span class="onoffswitch-inner"></span><span class="onoffswitch-switch"></span></label>\
    </div><span>ТН ON</span></div>\
<div class="menu-profiles">ПРОФИЛЬ\
    <br/>\
    <select id="get_listprofile" onchange="setParam(\'get_listProfile\',\'get_listprofile\');"></select>\
</div>');

document.write('\
<ul class="cd-accordion-menu">\
    <li class="index"><a href="index.html"><i></i>Состояние</a></li>\
    <li class="plan"><a href="plan.html"><i></i>Схема ТН</a></li>\
    <li class="heating gvs profiles scheduler has-children">\
        <input type="checkbox" name="group-1" id="group-1">\
        <label for="group-1"><i></i>Отопление и ГВС</label>\
        <ul>\
            <li class="heating"><a href="heating.html">Отопление</a></li>\
            <li class="gvs"><a href="gvs.html">ГВС</a></li>\
            <li class="profiles"><a href="profiles.html">Профили</a></li>\
            <li class="scheduler"><a href="scheduler.html">Расписание</a></li>\
        </ul>\
    </li>\
    <li class="setsensors sensorst sensorsp actuators eev relay invertor const has-children">\
        <input type="checkbox" name="group-2" id="group-2">\
        <label for="group-2"><i></i>Конфигурация ТН</label>\
        <ul>\
            <li class="sensorsp"><a href="sensorsp.html">Датчики</a></li>\
            <li class="sensorst"><a href="sensorst.html">Датчики температуры</a></li>\
            <li class="setsensors"><a href="setsensors.html">Привязка датчиков</a></li>\
            <li class="eev"><a href="eev.html">ЭРВ</a></li>\
            <li class="relay"><a href="relay.html">Реле</a></li>\
            <li class="invertor"><a href="invertor.html"><i></i>Инвертор</a></li>\
            <li class="const"><a href="const.html"><i></i>Константы</a></li>\
        </ul>\
    </li>\
    <li class="lan config system files time notice mqtt  has-children">\
        <input type="checkbox" name="group-3" id="group-3">\
        <label for="group-3"><i></i>Сервис</label>\
        <ul>\
            <li class="config"><a href="config.html">Опции ТН</a></li>\
            <li class="lan"><a href="lan.html">Сеть</a></li>\
            <li class="time"><a href="time.html">Дата/Время</a></li>\
            <li class="notice"><a href="notice.html">Уведомления</a></li>\
            <li class="mqtt"><a href="mqtt.html">MQTT</a></li>\
            <li class="system"><a href="system.html">Система</a></li>\
            <li class="files"><a href="files.html">Файлы</a></li>\
        </ul>\
    </li>\
    <li class="charts test modbus log freertos has-children">\
        <input type="checkbox" name="group-4" id="group-4">\
        <label for="group-4"><i></i>Отладка ТН</label>\
        <ul>\
            <li class="charts"><a href="charts.html">Графики</a></li>\
            <li class="test"><a href="test.html">Тестирование</a></li>\
            <li class="modbus"><a href="modbus.html">Modbus</a></li>\
            <li class="log"><a href="log.html">Журнал</a></li>\
            <li class="freertos"><a href="freertos.html">ОС RTOS</a></li>\
        </ul>\
    </li>\
    <li class="about"><a href="about.html"><i></i>О контроллере</a></li>\
</ul>');

document.write('\
<div class="dateinfo">\
    <div id="get_status"></div>\
    <div>&#128241 <a href="/mob/">Мобильная версия</a></div>\
</div>');
var extract = new RegExp('[a-z0-9-]+\.html'); 
var pathname = location.pathname;
pathmath = pathname.match(extract);
if(!pathmath) {var activeli = "index";} else {var activeli = pathmath.toString().replace(/\.html/g,"");}
var elements = document.getElementsByClassName(activeli);
var countElements = elements.length;
for(i=0;i<countElements;i++){document.getElementsByClassName(activeli)[i].classList.add("active");}
updateParam("get_status,get_WORK,get_listProfile");
window.onscroll = function() { autoheight(); }
