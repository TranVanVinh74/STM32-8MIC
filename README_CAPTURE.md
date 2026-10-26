# Thu WAV 8 mic trên STM32F411

Bản này chỉ thu âm thô: I2S2/1/3/5 → PCM16 xen kẽ 8 kênh → USART1 4 Mbps → WAV.
Firmware và Python phải cùng phiên bản giao thức `8MICPCM1`; không dùng script `AA 55` cũ.

## Chạy

1. Mở `FFT/B1/B1.uvprojx` trong Keil, chọn đúng STM32F411CEUx và **Rebuild all**.
   Project vẫn dùng ARM Compiler 5 và CMSIS/Device pack như repo ban đầu.
   Danh sách build đã bỏ các đường dẫn TIMER/SPI/TFT/AI không có trong repo và ADC không dùng.
   Không nạp lại các file `.axf`/`.hex` cũ trong `Objects`; chúng không đại diện cho source mới.
2. Nạp firmware, reset board. Thuần SWD: không bật SWO trace trên PB3 vì chân này dùng I2S3_CK.
3. Nối PA9 (USART1_TX) → RX của CH343G, nối chung GND, mức logic 3.3V.
4. Từ thư mục repo, chạy:

```powershell
python -m pip install -r Python/requirements.txt
python Python/record_8mic.py --port COM8 --seconds 5 --output array_8mic_test.wav
```

Nếu tên output đã tồn tại, chọn tên mới. WAV hoàn chỉnh có 8 kênh, 16 bit,
16.000 frame/giây danh định, đúng 80.000 frame cho 5 giây. Block cuối được cắt đúng số frame.
JSON đi kèm ghi sequence đầu/cuối, mức min/max/RMS và số mẫu chạm biên từng kênh.
RMS báo mức tín hiệu, chưa tự chứng minh mic hoạt động đúng hoặc các kênh đồng bộ pha.

Nếu CRC/sequence/header lỗi hoặc firmware báo lỗi DMA/I2S, chương trình dừng,
giữ `.partial.wav` chứa phần hợp lệ trước lỗi, JSON có `status: incomplete`.
Không chèn zero hoặc nối âm thầm qua block bị mất vì dữ liệu dùng cho DOA cần liên tục.
Python chỉ tìm lại đầu gói khi mới mở cổng; sau khi khóa gói đầu tiên, lỗi truyền là lỗi thu.
Timeout mặc định 5 giây không có gói hợp lệ. Ctrl+C cũng giữ phần thu dở.

## Chân và thứ tự kênh giữ nguyên

| WAV | Bộ thu | WS | CK | SD | AF |
|---|---|---|---|---|---|
| 1,2 | I2S2 master L,R | PB12 | PB13 | PB15 | 5 |
| 3,4 | I2S1 slave L,R | PA4 | PA5 | PA7 | 5 |
| 5,6 | I2S3 slave L,R | PA15 | PB3 | PB5 | 6 |
| 7,8 | I2S5 slave L,R | PB1 | PB0 | PB8 | 6 |

PB12/PB13 phải cấp WS/CK thật đến cả 8 mic và ba bộ slave (qua buffer nếu mạch có).
Mỗi cặp dùng chung SD với hai mức L/R đối nhau; không nối chung SD của bốn cặp.
Nếu dùng 74LVC244, kiểm tra nguồn, OE và sơ đồ từng nhánh clock.

## Những thay đổi

- `lib1/I2S.c/.h`: đặt rõ PLLI2SM=16, N=256, R=5 và I2SDIV=25, ODD=0,
  MCKOE=0. Theo HSI danh định: I2S clock 51.2MHz, BCLK 1.024MHz, WS 16kHz.
  Không đổi CPU 100MHz. HSI có sai số thực; đo WS nếu cần độ chính xác thời gian.
- DMA: giữ 4096 half-word/bộ, nửa buffer 512 frame stereo; đếm lần hoàn thành
  của từng bộ, kiểm tra đủ cả bốn, kiểm tra NDTR trước/sau copy và thứ tự HT/TC.
  DMA buffers là volatile. Phát hiện lỗi DMA, overrun I2S, block bị ghi đè hoặc timeout.
  Không chỉ chờ cờ I2S2 rồi xóa cờ ba slave như trước.
- Khởi động: giữ WS cao/CK thấp qua GPIO khi arm slave (theo điều kiện WS của
  errata ES0287), kiểm tra mức WS tại ba chân slave, sau đó trả chân về AF và bật master.
  Đây không phải phép đo xác nhận đồng bộ vật lý; cần kiểm tra cạnh chuyển GPIO/AF,
  slot đầu và độ trễ đường clock trên mạch thực. Bỏ 16 block đầu (512ms) để mic ổn định;
  bỏ block không sửa được lệch bit/slot nếu phần cứng khởi động sai.
- `lib1/UART.c`: giữ USART1/PA9/PA10 và 4Mbps, đặt rõ 8N1, không flow control.
  Giữ truyền blocking: khoảng 20.56ms/gói so với 32ms/block, chưa thêm UART DMA.
  Cần đo tổng copy+CRC+gửi dưới 32ms; nếu quá chậm firmware dừng với lỗi, không che mất mẫu.
- `src/main.c`: bỏ giao thức AA55, gửi header 32 byte + PCM. Khi lỗi, dừng I2S
  và lặp gói báo lỗi mỗi 250ms; sửa nguyên nhân rồi reset board.
- `lib1/audio_protocol.h`: serialize little-endian rõ ràng, CRC32 chung C/Python.
- `Python/record_8mic.py`: kiểm CRC, sequence, thông tin định dạng, timeout;
  ghi WAV streaming và báo thống kê. Không lọc, không đổi gain, không dịch kênh để che lỗi.
- `B1.uvprojx`: chỉ build source thu âm còn tồn tại; thêm header protocol,
  bỏ CMSIS-DSP không dùng, bật tạo HEX. Các file nguồn ADC cũ vẫn được giữ trong repo.

## Gói UART

Little-endian, header 32 byte (không dùng C struct có padding):

| Offset | Nội dung |
|---|---|
| 0..7 | ASCII `8MICPCM1` |
| 8..11 | sequence uint32, tăng theo block thu |
| 12..15 | sample rate uint32 = 16000 |
| 16..17 | channels uint16 = 8 |
| 18..19 | frames uint16 = 512; lỗi = 0 |
| 20..23 | payload bytes uint32 = 8192; lỗi = 0 |
| 24..27 | fault flags uint32, âm thanh hợp lệ = 0 |
| 28..31 | CRC-32/ISO-HDLC của header[0:28] nối với payload |
| 32.. | PCM signed 16-bit LE: mỗi frame lần lượt kênh 1 đến 8 |

CRC tương thích `zlib.crc32`, không dùng mặc định CRC peripheral STM32.
Đầu stream có thể bắt đầu ở sequence bất kỳ khi PC mở cổng; các gói sau phải liên tiếp.

Fault bits: 0..3 DMA lỗi theo thứ tự I2S2/1/3/5; 4..7 overrun tương ứng;
8 block sai thứ tự/bị ghi đè; 9 timeout; 10 WS chưa cao tại slave khi khởi động.

## Kiểm tra trên máy và trên board

```powershell
python -m unittest discover -s tests -v
```

Đã kiểm thử parser với dữ liệu chia nhỏ, magic trong PCM, header/CRC hỏng,
mất byte, mất/lặp block, sequence wrap, fault packet, WAV đúng 5 giây/thứ tự kênh.
Test dùng GCC nếu có để so sánh header/CRC thực tế từ C với Python.
Đã kiểm tra cú pháp C bằng GCC với CMSIS headers STM32F411 chính thức.
**Chưa build/link bằng Keil, chưa đo timing hoặc chạy trên board thật.**

Trên board cần kiểm tra:

1. WS khoảng 16kHz và CK khoảng 1.024MHz, 64 clock mỗi stereo frame.
2. Cả bốn DMA chạy; fault flags bằng 0; không dùng breakpoint khi đang thu.
3. Thu 5 giây, xem JSON complete, không CRC/sequence error.
4. Trong Audacity xem đủ tám kênh; thử âm gần từng mic để đối chiếu mapping.
5. Dùng logic analyzer kiểm tra Philips I2S, MSB và slot L/R trên cả bốn SD.
   CRC tốt chỉ chứng minh truyền UART đúng; không chứng minh dữ liệu mic đúng bit/pha.
6. Trước khi chạy SCORE, kiểm tra lệch mẫu giữa các bộ thu bằng tín hiệu kiểm thử
   và hình học mic biết trước. Không tự ý dịch một kênh chỉ vì nhìn dạng sóng thấy lệch.

Nguồn đối chiếu: RM0383 (DMA/I2S), DS10314 (alternate functions), ES0287 mục 2.11.6
(WS khi bật I2S slave), CMSIS STM32F411 và driver I2S của ST.
