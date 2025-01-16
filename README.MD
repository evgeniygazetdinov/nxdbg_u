
CopyInsert
# NXDBG - Nintendo Switch Debugger

NXDBG - это отладчик для Nintendo Switch, состоящий из двух основных компонентов:
- Сервис отладки (C) для Nintendo Switch
- GUI клиент (Python) для ПК

## Требования

### Для сервиса отладки (service)
- devkitPro с libnx
- Переменные окружения:
  - DEVKITPRO
  - DEVKITARM

### Для GUI клиента (debugger)
- Python 3.x
- PyQt5
- virtualenv (рекомендуется)

## Установка

1. Клонируйте репозиторий:
```bash
git clone https://github.com/yourusername/nxdbg.git
cd nxdbg
```

### 2.Соберите сервис отладки:
cd service
make

### 3.Настройте окружение Python и установите зависимости:
```
cd ../debugger
python -m venv env
source env/bin/activate
pip install PyQt5
```
### Использование
```
Загрузите service.nsp на Nintendo Switch
```

### Запустите GUI клиент с одним из следующих параметров:

1.Подключиться к процессу по PID:
- python MainWindow.py --pid <pid>
### Подключиться к процессу по Title ID:
 -python MainWindow.py --titleid <titleid>
2. Ждать запуска приложения:
-python MainWindow.py --nextlaunch
### Тестовый режим:
- python MainWindow.py --test
### Функции отладчика

- Подключение к запущенным процессам
Просмотр и модификация памяти

- Управление точками останова
Просмотр регистров и стека

- Пошаговое выполнение
Дизассемблирование ARM кода


============================================
```
Структура проекта
service/ - Сервис отладки на C
source/ - Исходный код сервиса
nxdbg.json - Конфигурация для создания KIP
debugger/ - GUI клиент на Python
MainWindow.py - Основной класс GUI
RemoteConnection*.py - Модули связи с сервисом
Adapter*.py - Адаптеры для отображения информации
mainwindow.ui - Qt дизайн интерфейса
```
Copyright © 2017 plutoo
