// Мобильное меню. Народный контроллер ТН.
// Пишется синхронно через document.write, как и десктопный menu.js:
// прошивка подменяет этот файл по имени для роли "user" (WebServer.ino:157).
var _MI = [
 ['index', '_index.html', 'Состояние'],
 ['plan',  '_plan.html',  'Схема ТН'],
 ['heat',  '_heat.html',  'Отопление'],
 ['gvs',   '_gvs.html',   'ГВС'],
 ['prof',  '_prof.html',  'Профили'],
 ['stat',  '_stat.html',  'Статистика'],
 ['chart', '_chart.html', 'Графики'],
 ['log',   '_log.html',   'Журнал'],
 ['rtos',  '_rtos.html',  'ОС RTOS'],
 ['about', '_about.html', 'О контроллере']
];
_mdraw(_MI);
