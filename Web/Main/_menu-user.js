// Мобильное меню для роли "user". Народный контроллер ТН.
// Прошивка подменяет им _menu.js (WebServer.ino:157). Набор экранов совпадает
// с белым списком: _index.html, _plan.html, _about.html.
var _MI = [
 ['index', '_index.html', 'Состояние'],
 ['plan',  '_plan.html',  'Схема ТН'],
 ['about', '_about.html', 'О контроллере'],
 ['login', '_index.html', 'Логин', 'NeedLogin=1']
];
_mdraw(_MI);
