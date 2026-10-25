# QuickDb Pro — SQL & AI Analytics

> **Dil:** Türkçe · [English →](./README.en.md)

**QuickDb Pro**, SQLite (ve sürücü eklentileriyle PostgreSQL/MySQL/ODBC) üzerinde
çalışan, **doğal dilden SQL üreten** bir masaüstü veritabanı analizi uygulamasıdır.
Qt 6 / C++17 ile yazılmıştır; hem geleneksel bir SQL istemcisi (şema ağacı, sorgu
editörü, düzenlenebilir grid, ER diyagramı, import/export) hem de yerel bir **AI
asistanı + analist** sunar. Veriler cihazda kalır — LLM olarak yalnızca yerel
[Ollama](https://ollama.com) (`http://localhost:11434`) kullanılır.

---

## ✨ Özellikler

### Veritabanı & SQL
- **Şema ağacı:** Tables / Views / Indexes / Triggers kategorileri, kolonlar ve tip/FK bilgisi.
- **Sorgu editörü:** sözdizimi renklendirme, satır numaraları, şema otomatik tamamlama (`Ctrl+Space`), seçili ifadeyi çalıştırma, `Ctrl/Cmd+Enter` ile çalıştırma, temel formatlama ve `EXPLAIN QUERY PLAN`.
- **Çoklu sonuç sekmeleri:** salt-okunur sorgu sonuçları ve **düzenlenebilir** tablo gezinme (CRUD: satır ekle/sil, ✔ Save / ↺ Revert — değişiklikler kaydedilene kadar staged).
- **ER Diyagramı:** tablolar ve foreign-key ilişkileri otomatik çizim, zoom/pan.
- **DDL diyalogları:** tablo oluşturma, kolon ekleme, view/index oluşturma.
- **Import / Export:** CSV, JSON, XML, SQL dump (canlı önizleme ile).
- **Sürücü-bağımsız bağlantı:** SQLite fiilen çalışır; QPSQL/QMYSQL/QODBC eklentisi takılıysa hazır.
- **Tema:** Açık / Koyu mod (`Ctrl+D`).

### AI Asistanı (çift modellü)
- **Sorgu üretim modeli:** doğal dil → tek bir salt-okunur SQL (varsayılan `qwen2.5-coder:3b`).
- **İş analisti modeli:** sonuç satırlarını Türkçe düzyazıya çevirir (varsayılan `qwen2.5:1.5b`).
- **Offline kural motoru (fallback):** Ollama kapalıyken TR/EN niyet eşleme (count/sum/trend/topN/lookup/select) ile yine de çalışır.
- **Analytics sekmesi:** otomatik eksen seçimi + çizgi/çubuk/pastal grafik ve deterministik yönetici raporu (KPI, trend, dağılım, yoğunlaşma, aykırı değerler).
- **Güvenli guard:** yalnızca `SELECT/WITH/PRAGMA/EXPLAIN` kabul edilir; yazma işlemleri reddedilir.
- **Niyet okuma:** "merhaba" gibi sohbet mesajları SQL üretmeden sohbet olarak yanıtlanır.
- **Halüsinasyon koruması:** analist yalnızca verilen kolon/değerleri kullanır; anlamlı sayısal veri yoksa yorum üretilmez.

### Web Dashboard (localhost)
- Sonuçları tek tıkla **web paneline sabitleme**: her sabitlenen sonuç, sidebar'da
  otomatik başlıklı (örn. sorgudaki ana tablo → "Orders") yeni bir sayfa olur.
- Sayfalar **tablo + otomatik çubuk grafik** içerir; tamamı sunucu tarafında HTML
  üretilir, harici varlık/JavaScript gerekmez, tamamen çevrimdışı çalışır.
- Tek tıkla açılır: yeşil **Dashboard / Web Panel** butonu sunucuyu başlatır ve
  tarayıcıda `http://127.0.0.1:<port>/` adresini açar.

---

## 🧱 Mimari

Katmanlı tasarım — widget'lar yalnızca `ui/` içinde; iş mantığı GUI'den bağımsız
test edilebilir:

```
┌─────────────────────────────────────────────┐
│  ui/    Qt Widgets — görünüm & etkileşim     │  tek widget katmanı
├─────────────────────────────────────────────┤
│  ai/    offline akıl yürütme (widget'sız)    │  LLM/kural → SQL → insight
├─────────────────────────────────────────────┤
│  core/  DatabaseManager + io + web (DB mantığı) │  sıfır widget bağımlılığı
├─────────────────────────────────────────────┤
│  Qt6::Sql → QSQLITE/QPSQL/... → veritabanı    │
└─────────────────────────────────────────────┘
```

| Dizin | İçerik |
|---|---|
| `src/core/` | `DatabaseManager` (bağlantı, sorgu, şema introspeksiyonu, DDL), `io/` (Exporters/Importers), `web/DashboardServer` (localhost HTTP paneli) |
| `src/ai/` | `SchemaContext`, `OllamaClient`, `RuleBasedNlu`, `DataInspector`, `AiOrchestrator` |
| `src/ui/` | `MainWindow`, `AiAssistantPanel`, `QueryEditor`, `ResultTab`, `SchemaTreeModel`, `ErDiagramView`, `Theme`, `Dialogs/` |
| `test/` | `smoke`, `io`, `ai`, `ui` (headless) + `e2e_*` canlı sürücüler |

Ayrıntılı dosya-dosya açıklama için: [TECHNICAL_GUIDE.md](./TECHNICAL_GUIDE.md) · Yol haritası: [DEVELOPMENT_PLAN.md](./DEVELOPMENT_PLAN.md)

---

## 🔧 Gereksinimler

- **CMake** ≥ 3.21
- **C++17** derleyici (Clang/Apple Clang veya GCC)
- **Qt 6** (6.9+ ile test edildi) — bileşenler: `Widgets Sql Charts Network Xml Test`
- *(isteğe bağlı, AI için)* **[Ollama](https://ollama.com)** kurulu ve çalışan bir model:
  ```bash
  ollama pull qwen2.5-coder:3b   # sorgu üretimi
  ollama pull qwen2.5:1.5b       # iş analisti yorumu
  ```
- *(isteğe bağlı, PostgreSQL/MySQL için)* ilgili Qt SQL sürücü eklentisi.

macOS'ta Qt kurulumu (Homebrew):
```bash
brew install qt
```

---

## 🚀 Derleme & Çalıştırma

```bash
# Yapılandırma (Qt yolunuzu gerekirse değiştirin)
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt

# Tüm hedefleri derle
cmake --build build -j4

# Uygulamayı çalıştır
./build/QuickDb.app/Contents/MacOS/QuickDb      # macOS
# veya:  open build/QuickDb.app
```

Linux/Windows için `CMAKE_PREFIX_PATH`'i Qt kurulum dizininize yönlendirmeniz
yeterlidir (ör. `-DCMAKE_PREFIX_PATH=~/Qt/6.9.0/clang_64`).

---

## ✅ Testler

```bash
cmake --build build -j4

./build/smoke_test                 # core/DB katmanı (GUI yok)
./build/io_test                    # import/export round-trip
./build/ai_test                    # offline AI mantığı (kural + analist)
QT_QPA_PLATFORM=offscreen ./build/ui_test   # headless GUI entegrasyon testi
```

**Canlı (ağ/Ollama gerektiren) sürücüler** — manuel çalıştırılır:
```bash
./build/e2e_ai_test samples/company_analytics.db   # uçtan uca AI hattı
./build/e2e_replay  <soru-dosyası>                # tek soruyu yeniden oynat
QT_PLUGIN_PATH=build ./build/pg_e2e_test          # PostgreSQL (kuruluysa)
```

---

## 📦 Örnek Veri

`samples/` altında hazır bir analitik veritabanı bulunur:
- `samples/company_analytics.db` — siparişler, ürünler, kategoriler, müşteriler
- `samples/company_analytics.sql` — aynı veritabanının kurulum script'i

Uygulamayı açtıktan sonra **File ▸ Open** ile `.db` dosyasını seçip örnek
sorguları/AI sorularını deneyebilirsiniz.

---

## 🖥️ Web Dashboard Kullanımı

1. Bir sorgu çalıştırın veya AI asistanına soru sorun (sonuç Results sekmesine gelir).
2. Üstteki CRUD satırında **"📌 Panoya Ekle"** kutusunu işaretleyin — sonuç,
   başlığı otomatik üretilerek panelde yeni sayfa olur.
3. **"🟢 Dashboard / Web Panel"** butonuna basın; tarayıcıda `http://127.0.0.1:<port>/`
   açılır. Sidebar'dan sayfalara geçebilirsiniz. Sunucu uygulama ile birlikte kapanır.

---

## ⚙️ Yapılandırma Notları

- **AI kapalıysa** uygulama otomatik olarak deterministik kural motoruna düşer;
  tüm SQL/analiz akışı çevrimdışı da çalışır.
- **Ollama adresi** sabit `http://localhost:11434` olarak yapılandırılmıştır — veri
  cihaz dışına çıkmaz.
- **Dashboard portu** tercihen `8765`; meşgulse uygun boş porta düşer.
- Kayıtlı bağlantı profilleri ve tercihler `QSettings` (`QuickDb`) altında saklanır.

---

## 📁 Proje Dizini

```
db_furcan/
├── src/{core,ai,ui}/        # uygulama kaynakları
├── test/                    # testler (smoke/io/ai/ui + e2e)
├── samples/                 # örnek veritabanı + script
├── CMakeLists.txt           # build tanımları
├── TECHNICAL_GUIDE.md       # dosya-dosya teknik rehber (TR)
├── DEVELOPMENT_PLAN.md      # yol haritası
├── README.md                # bu dosya (TR)
└── README.en.md             # İngilizce sürüm
```

---

## 📄 Lisans

Bu proje geliştirme/öğrenme amaçlı örnek bir uygulamadır. İstenirse bir açık
kaynak lisansı (ör. MIT) eklenebilir.
