# Copyright 2018 plutoo
from RemoteConnection import *
import usb.core
import usb.util



class RemoteConnectionUsb(RemoteConnection):
    def __init__(self):
        RemoteConnection.__init__(self)
        devices = list(usb.core.find(find_all=True))
        print("\nНайденные USB устройства:")
        for dev in list(devices):
            try:
                print(f"Vendor ID: 0x{dev.idVendor:04x}, Product ID: 0x{dev.idProduct:04x}")
                print(f"Bus: {dev.bus}, Address: {dev.address}")
                print(f"Port: {dev.port_number}, Speed: {dev.speed}")
                print("---")
            except:
                print("Не удалось получить информацию об устройстве")

        # Используем ID вашего устройства
        self.dev = usb.core.find(idVendor=0x18d1, idProduct=0x4ee0)

        if self.dev is None:
            raise Exception('Устройство не найдено')

        try:
            # Получим информацию об устройстве
            print(f"\nИнформация об устройстве:")
            print(f"Manufacturer: {usb.util.get_string(self.dev, self.dev.iManufacturer)}")
            print(f"Product: {usb.util.get_string(self.dev, self.dev.iProduct)}")
            
            # Сбросим устройство
            try:
                self.dev.reset()
                print("Устройство сброшено")
            except:
                print("Не удалось сбросить устройство")

            # Получим конфигурацию
            try:
                self.cfg = self.dev.get_active_configuration()
            except:
                print("Пробуем установить конфигурацию...")
                self.dev.set_configuration()
                self.cfg = self.dev.get_active_configuration()

            print(f"Активная конфигурация: {self.cfg.bConfigurationValue}")
            
            # Перечислим все интерфейсы
            print("\nДоступные интерфейсы:")
            for intf in self.cfg:
                print(f"Интерфейс {intf.bInterfaceNumber}:")
                for ep in intf:
                    print(f"  Endpoint 0x{ep.bEndpointAddress:02x}: "
                          f"{'IN' if usb.util.endpoint_direction(ep.bEndpointAddress) == usb.util.ENDPOINT_IN else 'OUT'}, "
                          f"тип: {ep.bmAttributes}, "
                          f"макс размер: {ep.wMaxPacketSize}")

            # Выберем первый интерфейс
            self.intf = self.cfg[(0,0)]
            print(f"\nИспользуется интерфейс: {self.intf.bInterfaceNumber}")

            # Найдем endpoints
            self.ep_in = usb.util.find_descriptor(
                self.intf,
                custom_match = \
                lambda e: \
                    usb.util.endpoint_direction(e.bEndpointAddress) == \
                    usb.util.ENDPOINT_IN)
            
            if self.ep_in is None:
                raise Exception("Не найден IN endpoint")
            print(f"IN endpoint: 0x{self.ep_in.bEndpointAddress:02x}")

            self.ep_out = usb.util.find_descriptor(
                self.intf,
                custom_match = \
                lambda e: \
                    usb.util.endpoint_direction(e.bEndpointAddress) == \
                    usb.util.ENDPOINT_OUT)
            
            if self.ep_out is None:
                raise Exception("Не найден OUT endpoint")
            print(f"OUT endpoint: 0x{self.ep_out.bEndpointAddress:02x}")
            
        except usb.core.USBError as e:
            print(f"Ошибка при инициализации USB: {str(e)}")
            raise

    def read(self, size):
        data = bytearray()
        timeout = 5000  # 5 секунд таймаут
        
        while size > 0:
            try:
                # Используем размер пакета из endpoint
                max_packet_size = self.ep_in.wMaxPacketSize
                chunk = self.ep_in.read(min(size, max_packet_size), timeout=timeout)
                if not chunk:
                    print("Предупреждение: получены пустые данные")
                    continue
                print(f"Получено {len(chunk)} байт: {' '.join([hex(b) for b in chunk])}")
                data.extend(chunk)
                size -= len(chunk)
            except usb.core.USBTimeoutError:
                print("Таймаут при чтении, повторная попытка...")
                continue
            except usb.core.USBError as e:
                print(f"Ошибка USB при чтении: {str(e)}")
                raise
        return bytes(data)

    def write(self, data):
        size = len(data)
        tmplen = 0
        timeout = 5000  # 5 секунд таймаут
        
        while size != 0:
            try:
                # Используем размер пакета из endpoint
                max_packet_size = self.ep_out.wMaxPacketSize
                chunk = data[:max_packet_size]
                print(f"\nОтправляем команду: {' '.join([hex(b) for b in chunk])}")
                tmplen = self.ep_out.write(chunk, timeout=timeout)
                print(f"Отправлено {tmplen} байт")
                size -= tmplen
                data = data[tmplen:]
            except usb.core.USBTimeoutError:
                print(f"Таймаут при записи в USB. Повторная попытка...")
                import time
                time.sleep(0.5)  # Увеличим задержку
                continue
            except usb.core.USBError as e:
                print(f"Ошибка USB при записи: {str(e)}")
                raise