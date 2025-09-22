# ESP32 BNO055 Veri Logger with SD Card and SNTP

Bu proje, ESP32 kullanarak BNO055 sensöründen hareket ve yön verilerini okumak, SNTP ile doğru zaman damgası eklemek ve verileri SD karta kaydetmek için geliştirilmiştir.

## Özellikler

- BNO055 sensöründen açısal hız, ivme ve yön verilerini okuma  
- SNTP üzerinden gerçek zamanlı zaman damgası alma  
- Verileri SD karta JSON veya CSV formatında kaydetme  
- Seri port üzerinden veri izleme ve hata takibi  

## Donanım Gereksinimleri

- ESP32 geliştirme kartı  
- BNO055 IMU sensörü  
- SD kart modülü   

## Kurulum

1. Depoyu klonlayın:  
   ```bash
   git clone https://github.com/KarakurtEnis/esp32-bno055-logger.git
