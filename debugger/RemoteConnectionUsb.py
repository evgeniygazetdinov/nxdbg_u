# Copyright 2018 plutoo
from RemoteConnection import RemoteConnection
import usb.core
import usb.util



class RemoteConnectionUsb(RemoteConnection):
    def __init__(self):
        RemoteConnection.__init__(self)
        self.debug = True
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
        # self.dev = usb.core.find(idVendor=0x18d1, idProduct=0x4ee0)
        # 057e:3000
        self.dev = usb.core.find(idVendor=0x057e, idProduct=0x3000)

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
            self.intf = self.cfg[(0, 0)]
            print(f"\nИспользуется интерфейс: {self.intf.bInterfaceNumber}")

            # Найдем endpoints
            self.ep_in = usb.util.find_descriptor(
                self.intf,
                custom_match= \
                    lambda e: \
                        usb.util.endpoint_direction(e.bEndpointAddress) == \
                        usb.util.ENDPOINT_IN)

            if self.ep_in is None:
                raise Exception("Не найден IN endpoint")
            print(f"IN endpoint: 0x{self.ep_in.bEndpointAddress:02x}")

            self.ep_out = usb.util.find_descriptor(
                self.intf,
                custom_match= \
                    lambda e: \
                        usb.util.endpoint_direction(e.bEndpointAddress) == \
                        usb.util.ENDPOINT_OUT)

            if self.ep_out is None:
                raise Exception("Не найден OUT endpoint")
            print(f"OUT endpoint: 0x{self.ep_out.bEndpointAddress:02x}")

        except usb.core.USBError as e:
            print(f"Ошибка при инициализации USB: {str(e)}")
            raise

    def write(self, data):
        if self.debug:
            print(f"\nОтправляем пакет размером {len(data)} байт:")
            print(f"Hex: {' '.join([f'{b:02x}' for b in data])}")
            print(f"ASCII: {' '.join([chr(b) if 32 <= b <= 126 else '.' for b in data])}")
        
        size = len(data)
        total_written = 0
        timeout = 5000
        retry_count = 3
        
        while size > 0 and retry_count > 0:
            try:
                # Используем размер пакета из endpoint
                max_packet_size = self.ep_in.wMaxPacketSize
                chunk = data[:max_packet_size]
                
                if self.debug:
                    print(f"\nОтправляем чанк {len(chunk)} байт:")
                    print(f"Hex: {' '.join([f'{b:02x}' for b in chunk])}")
                
                tmplen = self.ep_out.write(chunk, timeout=timeout)
                if tmplen != len(chunk):
                    print(f"Внимание: отправлено {tmplen} байт из {len(chunk)}")
                    retry_count -= 1
                    continue
                
                total_written += tmplen
                size -= tmplen
                data = data[tmplen:]
                
                # Ждем подтверждение от устройства
                try:
                    ack = self.ep_in.read(1, timeout=1000)
                    if self.debug:
                        print(f"Получено подтверждение: {' '.join([f'{b:02x}' for b in ack])}")
                except:
                    print("Не получено подтверждение от устройства")
                    retry_count -= 1
                    continue
                
            except usb.core.USBTimeoutError:
                print(f"Таймаут при записи. Осталось попыток: {retry_count}")
                retry_count -= 1
                import time
                time.sleep(0.5)
                continue
            except usb.core.USBError as e:
                print(f"Ошибка USB при записи: {str(e)}")
                raise
        
        if retry_count == 0:
            raise Exception(f"Не удалось записать данные после нескольких попыток. Записано {total_written} из {len(data)} байт")
        
        return total_written

    def read(self, size):
        if self.debug:
            print(f"\nЧитаем {size} байт...")
        
        data = bytearray()
        timeout = 5000
        retry_count = 3
        
        while size > 0 and retry_count > 0:
            try:
                max_packet_size = self.ep_in.wMaxPacketSize
                chunk = self.ep_in.read(min(size, max_packet_size), timeout=timeout)
                
                if not chunk:
                    print("Предупреждение: получены пустые данные")
                    retry_count -= 1
                    continue
                
                if self.debug:
                    print(f"Получено {len(chunk)} байт:")
                    print(f"Hex: {' '.join([f'{b:02x}' for b in chunk])}")
                    print(f"ASCII: {' '.join([chr(b) if 32 <= b <= 126 else '.' for b in chunk])}")
                
                data.extend(chunk)
                size -= len(chunk)
                
                # Отправляем подтверждение
                try:
                    self.ep_out.write(b'\x06', timeout=1000)  # ACK
                except:
                    print("Не удалось отправить подтверждение")
                
            except usb.core.USBTimeoutError:
                print(f"Таймаут при чтении. Осталось попыток: {retry_count}")
                retry_count -= 1
                continue
            except usb.core.USBError as e:
                print(f"Ошибка USB при чтении: {str(e)}")
                raise
        
        if retry_count == 0:
            raise Exception(f"Не удалось прочитать данные после нескольких попыток. Прочитано {len(data)} из {size} байт")
        
        return bytes(data)