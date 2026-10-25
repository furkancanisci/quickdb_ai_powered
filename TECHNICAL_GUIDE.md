# QuickDb — Teknik Dosya Rehberi

Bu belge projedeki her dosyanın ne işe yaradığını, birbirleriyle nasıl
konuştuğunu ve neden bu şekilde tasarlandığını açıklar.
Dokümantasyon: Türkçe · Kod/terminoloji: İngilizce.

---

## Genel Mimari (Katmanlar)

```
┌─────────────────────────────────────────────┐
│  ui/  (Qt Widgets — görünüm & etkileşim)    │  sadece UI katmanı widget kullanır
├─────────────────────────────────────────────┤
│  ai/  (offline akıl yürütme — widget'sız)   │  LLM/kural → SQL → insight
├─────────────────────────────────────────────┤
│  core/ (DatabaseManager — tüm DB mantığı)   │  sıfır widget bağımlılığı
├─────────────────────────────────────────────┤
│  Qt6::Sql  →  QSQLITE driver  →  .db dosyası│
└─────────────────────────────────────────────┘
```

Temel kural: **hiçbir widget doğrudan SQL yazmaz / QSqlDatabase tutmaz.**
Tüm veritabanı işlemleri `core::DatabaseManager` üzerinden geçer. Bu sayede
logic, GUI'den bağımsız test edilebilir (bkz. `smoke_test`). `ai/` katmanı da
widget'sızdır; sadece `core`'a dokunur, böylece `ai_test` ile ağa/başlığa
dokunmadan test edilir.

---

## Dosya Dosya Açıklama

### 🔧 Build

| Dosya | Görevi |
|---|---|
| `CMakeLists.txt` | Projeyi tanımlar. 5 hedef üretir: **QuickDb** (GUI app), **smoke_test** (konsol/core testi), **ui_test** (headless GUI testi), **ai_test** (offline AI mantık testi), **io_test** (dosya-I/O round-trip testi). `find_package(Qt6 … Widgets Sql Test Charts Network Xml Gui Core)`. `AUTOMOC` sayesinde `Q_OBJECT` içeren her header için moc otomatik çalışır. Tema resource'suz (programmatic QSS) olduğu için `.qrc`/`rcc` gerekmez. Qt yolu: `-DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt`. |

### 🚀 Giriş Noktası

| Dosya | Görevi |
|---|---|
| `src/main.cpp` | `QApplication` kurar, `ui::MainWindow`'ı gösterir. `raise()+activateWindow()` ile öne alır (macOS'ta dialog'ların arkada kalmasını engeller). |

### 🗄️ Core Katmanı

| Dosya | Görevi |
|---|---|
| `src/core/DatabaseManager.h` | UI'dan bağımsız tüm DB sözleşmesi: `ColumnInfo/ColumnSpec/TableDefinition` yapıları, `ForeignKeyInfo` (FK ilişkisi), `QueryResult` (başarı/hata/süre/etkilenen satır/sorgu). **M7 ekleri:** `ViewInfo/IndexInfo/TriggerInfo` (şema nesneleri) ve sürücü-bağımsız `ConnectionConfig { driverType,filePath,host,databaseName,userName,password,connOptions,port,title, isSqlite() }`. |
| `src/core/io/Exporters.h/.cpp` | **Widget'sız serileştirme (M7-E).** `ExportInput {columns,rows,tableName}` → `toCsv` (RFC-4180: delimiter/tırnak/yeni-satira sahip alanlar quote'lanır), `toJson` (satır başına kolon-adıyla nesne; tam metin round-trip için değerler string, boş hücre → null), `toXml` (`QXmlStreamWriter`, `<table name="…"><row><col>…` ), `toSqlDump` (`BEGIN TRANSACTION;` + `INSERT INTO "tablo" (…) VALUES (…)` + `COMMIT;`, NULL hücreler SQL NULL). Her fonksiyon dokümanı `QString` döndürür → diske/önizlemeye/test'e akabilir. XLSX bilinçli olarak kapsam dışı (harici lib). |
| `src/core/io/Importers.h/.cpp` | **Dosya → tablo ayrıştırma (M7-E).** `ImportedTable {ok,error,columns,rows}` + `parseCsv(path,delim,hasHeader)` (tırnak içindeki yeni-satırı yeniden gruplar) ve `parseJson(path)` (dizi-nesne VEYA ilk dizi-üyesi olan nesne kabul eder). `inferColumnType()` INTEGER/REAL/TEXT sezimi; `buildCreateTable()` + `buildInserts()` (transaction'lı) ile hedef tablo DDL'i üretilir. |
| `src/core/DatabaseManager.cpp` | Uygulama detayları: <br>• **Bağlantı yaşam döngüsü:** her açılışta benzersiz isimli (`quickdb_conn_N`) QSqlDatabase; kapanışta query'ler bırakılıp `removeDatabase` çağrılır (sızıntı uyarısı yok). <br>• **`splitStatements()`:** SQL script'ini `;` ile parçalar; tırnak (`' " `) ve yorum (`-- /* */`) içindeki `;`'yı bilir — düzensiz split kaynaklı hataları engeller. <br>• **`runScript()/runSingle()`:** QElapsedTimer ile ms süner, QSqlError'ı yakalar (exception yerine `QueryResult.errorMessage` — çökme yok). <br>• **Şema introspeksiyonu:** `tableNames()` (sqlite_master), `columnsOf()` (PRAGMA table_info) ve `foreignKeysOf()` (PRAGMA foreign_key_list) → sol ağacı ve ER diyagramını besleyen "schema inspector". <br>• **DDL builder:** `createTable/addColumn/dropTable`; identifier'lar `quoteIdentifier()` ile `"..."` sarılır (SQL injection'a ve boşluklu/turuncu isimlere karşı güvenli). <br>• **Sürücü-bağımsız bağlantı (M7-D):** `connect(ConnectionConfig)` seçilen Qt SQL driver'ını + alanları kurar (fiilen `QSQLITE` çalışır; `QPSQL/QMYSQL/QODBC` plugin takılınca hazır); `availableDrivers()` = `QSqlDatabase::drivers()`; `currentConfig()` aktif cfg'yi döndürür. `createDatabase/openDatabase` korunur. <br>• **Şema nesneleri (M7-F):** `views()` (sqlite_master type='view'), `indexesOf(table)` (`PRAGMA index_list`/`index_info`; autoindex/constraint'leri işaretler), `triggersOf(table)`, `sqlFor(obj)`; `createView/dropView`, `createIndex/dropIndex`, `createTrigger(ddl)/dropTrigger`. <br>• **`explainQueryPlan(sql)` (M7-G):** `;`'yı sıyırır, zaten `explain` ile başlamıyorsa başına `EXPLAIN QUERY PLAN ` ekler, satırları `QueryResult` olarak döndürür. |

### 🧠 AI Katmanı (offline — `src/ai/`)

Widget'sız, saf mantık katmanı. Tüm dosyalar `ai::` isim alanında.

| Dosya | Görevi |
|---|---|
| `src/ai/SchemaContext.h/.cpp` | `DatabaseManager`'dan tablo/kolon/tip/FK dökümü + kolon başına ≤3 örnek değer. LLM prompt'unu (`toPromptText()`) ve kural motorunu besler. `bestTableForQuestion()` sorudaki tablo adını/kolon eşleşmesini bulur; `looksNumeric/looksDateish` tip sezgileri; `foldTr()` Türkçe karakter katlaması (ı→i, ş→s…). Elle kurulabilir (testte DB'siz). |
| `src/ai/OllamaClient.h/.cpp` | `QNetworkAccessManager` ile yalnızca `http://localhost:11434`'e non-blocking istek. `probe()` (GET /api/tags) kullanılabilirliği VE kurulu model adlarını (`installedModels()`) döndürür, `generateSql()` (POST /api/generate, `stream:false`) SQL üretimini yapar; **180 sn timeout** + `options.num_predict=8192` (uzun/iç içe SQL için) + düşük `temperature=0.1`. Varsayılan model `qwen2.5-coder:3b` (~1.9 GB — ≤2 GB bütçesine sığan en güçlü kod/SQL modeli; kurulu değilse panel en iyi kurulu modeli seçer). Sistem prompt'u tek bir salt-okunur SQL'i ```sql``` bloku içinde, **kısaltmadan/tam** ister. **Veri asla cihaz dışına çıkmaz.** |
| `src/ai/RuleBasedNlu.h/.cpp` | Deterministik offline kural motoru (hizmet gerektirmez). Regex tabanlı TR/EN niyet eşleme: count/sum/trend/topN/lookup/select. `run()` niyeti bulur, şemadan tablo+kolon çözer, `quoteIdentifier` ile güvenli SQL kurar. `isReadOnlySql()` yalnızca select/with/pragma/explain kabul eder; insert/update/delete/drop… reddeder (tırnak literacy'leri önce silinir). `extractSqlFromText()` LLM çıktısından ```fence```/`SQL:`/SELECT span'ını çeker. |
| `src/ai/DataInspector.h/.cpp` | `TableData` (sonuç seti string'leşmiş kopyası) + `Insight`. `analyze()` X/Y eksenlerini ve grafik tipini seçer (tarih/ardışık ≥6 → Line, ≤8 kategorisel → Pie, aksi → Bar) ve toplam/maks/min/ilk→son %değişim üzerinden TR/EN yönetici özetini yazar. **M7-C analist raporu:** `Insight`'a `reportHtmlTr/reportHtmlEn` eklendi — deterministik istatistik motoruyla (yardımcı `Stats` + quantile/stdev) KPI'lar (satır/distinct/toplam/ortalama/medyan/min/max/stddev), trend (ilk→son %değişim/yön), dağılım (ilk 3 katkıda bulunan +%pay, yoğunlaşma), aykırı değerler (mean±2σ), kategori kırılımı ve öneri cümlelerini kendi kendine yeter HTML olarak üretir. Deterministiktir, LLM kullanmaz. |
| `src/ai/AiOrchestrator.h/.cpp` | Akışın orkestratörü: `ask()` → (LLM açıksa ve hazırsa) üret → guard → değilse `fallbackToRules()` → `runSql()` mevcut `runSingle` ile çalıştır → `TableData` + `analyze`. **LLM SQL'i guard'ı geçerse her zaman gösterilir; çalıştırılamazsa sessizce kural motoruna düşürülmez** (yalnızca LLM hiç güvenli SQL üretmezse fallback) — panel gerçek SQL'i + DB hatasını gösterir. `answerReady(Answer)` sinyali; `Answer{question,generatedSql,source,message,result,data,insight}`. `Q_DECLARE_METATYPE` ile kuyruklu bağlantı/test. **M7-C otomatik-present akışı:** SQL test/execute edilip başarılıysa panel `presentResult(...)` yayar → `MainWindow` SQL'i editöre koyar ve **aynı sonucu** yeniden sorgulamadan yeni sonuç sekmesinde gösterir (salt-okunur değilse yalnız editöre kopyalanır, çalıştırılmaz). |

### 🖼️ UI Katmanı


| Dosya | Görevi |
|---|---|
| `src/ui/MainWindow.h/.cpp` | Orkestrasyon merkezi. **3 sütunlu QSplitter(H):** sol şema ağacı · orta **sekmeler: "SQL & Data"** (üst: grid, alt: editör+Execute) ve **"ER Diagram"** · sağ `ui::AiAssistantPanel`; status bar (AI motor rozeti dahil: "AI: Ollama (model)" / "AI: Offline Kural Motoru"). <br>• **Çift modlu grid:** tabloya tıklandığında `enterBrowseMode()` editable `QSqlTableModel` (`OnManualSubmit` — değişiklikler ✔ Save'e kadar staged), Execute'te `enterQueryMode()` ile salt-okunur `SelectQueryModel`'e döner. CRUD slot'ları: `addRowToTable/deleteCurrentRow/saveTableChanges/revertTableChanges` + grid sağ tık menüsü (`onResultContextMenu`: Add/Delete/Save/Revert/Copy Cell). <br>• `setupUi()` QSplitter+QTabWidget layoutsunu kurar (`setSizes({260,660,320})`); `setupActions()` her QAction'ı slot'a bağlar, `Ctrl+Return` çalıştırma ve View menüsünde "AI Assistant" (`Ctrl+Shift+A`) görünürlük kısayolunu tanımlar. <br>• **AI wiring:** panelin `insertSqlRequested(sql)` sinyali `onAiInsertSql()`'e (SQL'i editöre koy + Çalıştır); `OllamaClient::availabilityChecked` → `updateAiBadge()`; `executeSql()` sonrası sonuç modeli `showResultFromModel()` ile Analytics'e beslenir. <br>• **Test edilebilirlik:** `objectName`'ler (`executeButton`, `queryEditor`, `schemaTree`, `resultView`, `connectionLabel`, `insertRowButton`, `deleteRowButton`, `saveChangesButton`, `revertButton`, `browseLabel`, `rightTabs`, `erView`, `aiPanel`, `aiChat`, `aiInput`, `aiSendButton`, `aiChartView`, `aiSummary`) ve `connectToPath` private-slot — headless test bunları bulup tetikliyor. <br>• `setNativeMenuBar(false)` — macOS native menüsünün eylemleri yutma riskine karşı menüler pencere içi. <br>• `executeSql()`: boş/hatalı guard'lar → status bar'a ✔/✖ mesajı + ms süresi; DDL ise ağacı otomatik `refreshSchema()`. <br>• Slot'larda `qDebug()` logları → `build/quickdb.log`'dan tıklamalar doğrulanabilir. <br>• **M7-B çoklu sonuç sekmeleri:** tek grid yerine `QTabWidget *m_resultTabs` (tabsClosable+movable); `openTableTab(table)` (zaten açıksa ona geçer+`select()`), `openQueryTab(sql, QueryResult)` yeni sekme açar; `currentResultView()` aktif sekmenin grid'ini döndürür (test bunu kullanır); dirty sekme kapanışında onay sorulur. <br>• **M7-D/E/F/G wiring:** ConnectionDialog/Manager (File menüsü, `Ctrl+Shift+C`), `exportActiveResult`/`exportSelectedTable`/`importFile` + `collectExportData()` (`Ctrl+Shift+E`/`Ctrl+Shift+I`), ağaç/diğer sağ tık menüleri; `openViewTab`, `createViewViaDialog`/`createIndexViaDialogForTable`/`showObjectDdl`/`dropTreeObject`; `explainCurrentQuery` (`Ctrl+Shift+X`) ve `formatCurrentSql` (`Ctrl+Shift+F`) Query menüsü+toolbar; `refreshSchema()` editör completion'larını doldurur + `expandToDepth(1)`. |
| `src/ui/SchemaTreeModel.h/.cpp` | `QAbstractItemModel` subclass'ı. **M7-F:** ağaç artık DB altında kategori düğümleriyle: **Tables / Views / Indexes / Triggers** (ikonlu). `NodeType { None, DatabaseRoot, Category, Table, Column, View, Index, Trigger }`; `refresh()` view adlarını `tableNames()`'ten ayırıp (QSet) kategorileri kurar, index/trigger'ları tablo başına PRAGMA/sqlite_master'dan ekler (autoindex/constraint index'ler atlanır). `objectForIndex()` → `TreeObject{type,name,table}` sağ tık menüsüne hedef nesne verir. Node'lar `std::unique_ptr<Node>` ağacı (RAII); `beginResetModel/endResetModel` ile atomik yenileme. |
| `src/ui/QueryEditor.h/.cpp` | `QPlainTextEdit` tabanlı monospace editör (Menlo). <br>• `LineNumberArea`: solda satır numarası gutter'ı, aktif satır highlight. <br>• `keyPressEvent`: **Ctrl/Cmd+Enter → `executeRequested` sinyali**; Tab 4 boşluk. <br>• `statementToExecute()`: seçim varsa sadece seçimi çalıştırır (DBeaver mantığı). <br>• **M7-G:** kurucuda `ui::SqlHighlighter(document())` bağlanır; `QCompleter`+`QStringListModel` popup ile şema auto-complete (`setSchemaCompletions()` tablo adları + `table.column`; `refreshSchema`'den beslenir), kelime/`.` tetikleyicili `maybeComplete()`, `Ctrl+Space` elle tetikleme, Up/Down/Enter/Tab/Esc popup'a yönlendirilir; `formatSql()` temel keyword-newline formatlama. |
| `src/ui/SqlHighlighter.h/.cpp` | **M7-G.** `QSyntaxHighlighter`: SQL anahtar kelimeleri (bold mavi), fonksiyonlar (`(` öncesi tanımlayıcı), sayılar, string literalleri (`'…'`/`"…"`), satır yorumu (`--`) ve çok satırlı blok yorum (`/* */`, `setCurrentBlockState(1)` ile durum taşır). `QRegularExpression` kuralları by-value tutulduğu için header `<QRegularExpression>` dahil eder. |
| `src/ui/ResultTab.h/.cpp` | **M7-B.** Çoklu sonuç sekmesinin tek birimi (`QWidget`): kendi `QTableView` + CRUD toolbar'ı (Add/Delete/Save/Revert + browseLabel). İki mod: `Query` (salt-okunur `SelectQueryModel`) veya `Browse` (düzenlenebilir `QSqlTableModel`, `OnManualSubmit`); AI-snapshot 3. kaynak. `tableOrSql()`, `activeModel()`, `isDirty()`, `clearData()` (kapanışta model+query bırakılır). `MainWindow` aktif sekmeye CRUD slot'larını yönlendirir. |
| `src/ui/Theme.h/.cpp` | **M7-A.** Uygulama-merkezli tema yardımcısı (resource/.qrc'siz, programmatic palette+QSS — bu yüzden her hedefe temiz linklenir). `apply(QApplication*, Mode)` açık/koyu palette + global QSS (QGroupBox/QMenuBar/QToolBar/QPushButton/QTreeView/QTabBar/QTableView/QLineEdit/chat balonları…), `current()`, `toggle()`. `MainWindow` View menüsünde "Dark Mode" (`Ctrl+D`). |
| `src/ui/SelectQueryModel.h/.cpp` | `QSqlQueryModel` subclass'ı (dosya şu an açık olan bu). <br>• `data()`: BLOB'u `(BLOB · n bytes)` gösterir, NULL hücreleri gri boyar — ham byte'ların grid'i çökertmesini engeller. <br>• `setSource(query, db)`: çalışan sorguyu VE kaynak SQL'i + bağlantı handle'ını saklar. <br>• `sort()`: base SQL'i `SELECT * FROM (…) ORDER BY "col"` olarak sarıp yeniden çalıştırır — QSqlQueryModel'de sort varsayılan olarak **no-op** olduğu için bu yazıldı; kolon başlığı tıklamaları gerçekten sıralar. <br>• `clearAll()`: `clear()` + saklanan handle'ı bırakır (yoksa `removeDatabase: connection still in use` uyarısı). |
| `src/ui/Dialogs/CreateTableDialog.h/.cpp` | Görsel DDL: tablo adı + kolon tablosu (name/type/NOT NULL/PK/default). `definition()` → `core::TableDefinition`; OK yolunda `DatabaseManager::createTable` bunu `CREATE TABLE` DDL'ine çevirir (multi-PK → composite constraint, INTEGER PK → AUTOINCREMENT). İlk satır "id" olarak PK işaretli gelir. |
| `src/ui/Dialogs/AddColumnDialog.h/.cpp` | `ALTER TABLE … ADD COLUMN` sihirbazı: tablo combo'su (ağaçtan ön seçimli), kolon adı/tip/default. |
| `src/ui/Dialogs/ConnectionDialog.h/.cpp` | **M7-D.** Sürücü-bağımsız bağlantı diyalogu: driver combo'su (`availableDrivers()`) + koşullu alanlar (SQLite: dosya; sunucu: host/port/db/user/pass — `userName` alanına dikkat), "Test Connection", kayıtlı profiller (`QSettings` `QuickDb/connections`). `config()` → `core::ConnectionConfig`. `MainWindow`'de objectName'ler: `connDriver`, `connFilePath`, `connHost`, `connTest`. |
| `src/ui/Dialogs/ExportDialog.h/.cpp` | **M7-E.** Kaynak (aktif sekme/tablo) + format combo'su (csv/json/xml/sql) + hedef dosya + canlı önizleme; `core::io::Exporters` ile üretir, diske yazar. objectName'ler: `exportFormat`, `exportPreview`. |
| `src/ui/Dialogs/ImportDialog.h/.cpp` | **M7-E.** Dosya seçimi, delimiter, header on/off, hedef tablo (var/yeni), kolon eşleme + önizleme + run; `core::io::Importers` ile parse, `buildCreateTable`/`buildInserts` (kullanıcı onayıyla) ile DB'ye yazar. objectName'ler: `importFile`, `importDelim`, `importHeader`, `importTable`. |
| `src/ui/Dialogs/CreateViewDialog.h/.cpp` | **M7-F.** Ad + `SELECT` gövdesi → `CREATE VIEW` (DatabaseManager::createView). |
| `src/ui/Dialogs/CreateIndexDialog.h/.cpp` | **M7-F.** Tablo + kolon seçimi + unique → `CREATE INDEX` (DatabaseManager::createIndex). |
| `src/ui/ErDiagramView.h/.cpp` | Salt-okunur ER sekmesi (`QGraphicsView`). `rebuild()`: her tabloyu kutu (ad + kolonlar, PK 🔑 rozeti) olarak otomatik grid layout'a çizer; her FK için child→parent kesikli ok + `kolon → kolon` etiketi (`foreignKeysOf()` verisiyle). Tekerlek zoom, sürükle pan; `fitInView` ile sığdırma. `tableCount()/relationCount()` test/doğrulama için dışa açık. |
| `src/ui/AiAssistantPanel.h/.cpp` | Sağ AI dock'u (`QTabWidget`, objectName `aiPanel`), kendi `ai::AiOrchestrator`'ını sahiplenir. <br>• **Chat sekmesi:** `QTextBrowser` balonları (kullanıcı sağ/yeşil, AI sol/beyaz HTML), `QPlainTextEdit` input + "Sor" butonu; `eventFilter` ile Enter(⇧'siz)=gönder. AI balonunda üretilen SQL + kaynak/satır/ms + `quickdb-sql://` "Editöre kopyala" linki (`anchorClicked` → `insertSqlRequested`). **Model picker (`aiModel` combo):** `availabilityChecked` ile kurulu Ollama modellerini listeler, coder/qwen tercih edip NER modelini otomatik seçmez; seçim `OllamaClient::setModel`'e gider. **Dürüst hata balonu:** `executed==false` ama SQL üretildiyse, balon SQL'i (kırmızı kod bloğu) + gerçek `result.errorMessage` + kopyala linkini gösterir — "SQL üretti" deyip SQL'i gizlemez. <br>• **Analytics sekmesi:** `QChartView` (QtCharts) + grafik tipi combo'su (Auto/Line/Bar/Pie) + X/Y kolon combo'ları + TR/EN dil toggle + özet `QLabel`. `paintChart()` satır/pie/bar serilerini insight'ten çizer; `loadDataset()` combo'ları doldurur, `rebuildAnalytics()` seçime göre yeniden analiz eder. **M7-C:** Analytics sekmesi tek `QLabel` yerine üstte `QChartView` + alta kaydırılabilir analist raporu (`QTextBrowser`, `reportHtmlTr/En`; TR/EN toggle korunur). Chat balonları rol etiketi ("Sen"/"AI") + saat damgası + avatar + yumuşak CSS; üretim sırasında "düşünüyor…" göstergesi, otomatik alta kaydırma, çok satırlı input (`Ctrl+Enter`/`Enter` gönderim). Başarılı SQL sonucunda `presentResult(...)` sinyali yayar (otomatik present → `MainWindow`). <br>• `showResultFromModel(QSqlQueryModel*)` manuel Execute sonuçlarını (query cursor'ını tüketmeden) Analytics'e besler; `onAnswer()` sonrası panel otomatik Analytics sekmesine geçer. |

### ✅ Testler

| Dosya | Görevi |
|---|---|
| `test/smoke_main.cpp` | **Core katman testi (GUI'siz):** DB oluştur, tablo kur, çoklu-statement INSERT, SELECT satır sayısı, PRAGMA şema okuma, add/drop column, hatalı SQL → hata mesajı, UPDATE/DELETE affected rows, close. **M7:** + view/index/trigger create → introspect → drop round-trip'leri, + `EXPLAIN QUERY PLAN`. **27 kontrol.** |
| `test/io_main.cpp` | **M7-E dosya-I/O round-trip testi (offscreen, hedef: `io_test`):** CSV/JSON/XML/SQL-dump üretimi + CSV/JSON parse eşlemesi, tırnaklı-alan (RFC-4180 gömülü yeni-satır) işleme, tip çıkarımı (INTEGER/REAL/TEXT). **26 kontrol.** |
| `test/ui_main.cpp` | **Headless entegrasyon testi (offscreen):** gerçek `MainWindow` widget'larını bulup **fiziksel olarak tıklar** (`button->click()`), Ctrl+Enter'ı `QTest::keyClick` ile basar, slotları `QMetaObject::invokeMethod` ile tetikler, ağaç/grid/diyalog çıktılarını doğrular. **34 kontrol** — CRUD, ER (2 tablo / 1 FK ok) ve **AI paneli** (canlı LLM'e bağımlı OLMADAN, `setUseLlm(false)` ile kural motoru): sekmeler, doğal dilden 6 satırlık trend grafiği, Analytics özeti, otomatik sekme geçişi, yıkıcı SQL'nin asla çalıştırılmadığı kanıtı. **M7 ekleri:** çoklu sonuç sekmeleri (2 tablo → 2 sekme, close, dirty onayı), CRUD'un aktif sekmede çalışması, AI auto-present → grid + editör SQL, kategorili şema ağacı (Tables/Views/Indexes/Triggers — `dbRoot`/`category`/`tableNode` helper lambda'ları), ConnectionDialog/ExportDialog/ImportDialog widget'ları, highlighter/completer `findChild`, `explainCurrentQuery`/`formatCurrentSql`. **52 kontrol.** |
| `test/ai_main.cpp` | **Offline AI mantık testi (ağsız, widget'sız):** şema eşleme, her niyet → SQL şekli (count/sum/trend/topN/lookup + EN varyantları), salt-okunur guard kabul/red matrisi, ```fence```'den SQL çıkarma, DataInspector Line/Bar/Pie kararları ve TR/EN özetler, fixture DB üzerinde uçtan uca orkestratör (LLM kapalı → Kural Motoru + çizilebilir insight). **M7:** + analist `reportHtmlTr/En` içerik kontrolleri (KPI/dağılım/öneri metinleri). **27 kontrol.** |

---

## Veri Akışı Örneği: "Execute" Tıklaması

```
executeButton.clicked ─┐
editor Ctrl+Enter ─────┼─► MainWindow::executeSql()
Query menü Ctrl+Return ┘        │
                                ▼
                  QueryEditor::statementToExecute()   (seçim veya hepsi)
                                │
                                ▼
                  DatabaseManager::runScript(sql)
                     ├─ splitStatements() (tırnak/yorum-aware)
                     ├─ her ifade: QSqlQuery::exec + QElapsedTimer
                     └─ QueryResult { success, error, ms, query }
                                │
              ┌─────────────────┼──────────────────┐
              ▼                 ▼                    ▼
        hasResultSet?     başarı (DDL/DML)      hata
   setSource() → grid    affected rows + ms   status bar ✖ mesajı
              │                 │                    │
              └────► status bar ✔/✖ + süre ◄─────────┘
                     DDL ise → SchemaTreeModel::refresh()
```

## Veri Akışı Örneği: AI Chat'te Doğal Dil → Grafik

"Son 6 ayın satış trendini göster ve özetle" (LLM kapalıysa kural motoru çalışır):

```
AiAssistantPanel (Chat) ── askQuestion() ──► ai::AiOrchestrator::ask(soru)
        │
        ├─ LLM açık & Ollama hazır? ──► OllamaClient::generateSql(schema+soru)
        │                                     └─ response ─► extractSqlFromText() ─► isReadOnlySql()?
        │                                             (güvenliyse)                       (değilse ↓)
        └─ offline fallback ─► RuleBasedNlu::run(soru, SchemaContext)  ─► Plan{sql}
                                              │
                                              ▼
                     DatabaseManager::runSingle(sql)  ─► QueryResult
                                              │
                                              ▼
                     TableData::fromQueryResult ─► DataInspector::analyze()
                       {chartType, x, y, özetTR, özetEN}
                                              │
                    ┌─────────────────────────┴───────────────┐
                    ▼                                           ▼
        Chat balonu (SQL + kaynak/satır/ms +            Analytics sekmesi:
        "Editöre kopyala" linki)                        QChartView (Line/Bar/Pie)
                                                         + TR/EN yönetici özeti
```

Not: `MainWindow`, manuel Execute sonuçlarını da `showResultFromModel()` ile
Analytics'e besler (aynı QueryResult'ı tekrar sorgulamadan, grid'in cursor'ını
tüketmeden).

## Veri Akışı Örneği: AI Auto-Present (chat → editör → sonuç grid'i) — M7-C

```
AiOrchestrator SQL üretir → isReadOnlySql() guard → runSingle() test/execute
        │  (başarılı & salt-okunur)
        ▼
AiAssistantPanel ::presentResult(sql, TableData, source, message)
        │
        ▼
MainWindow::onAiPresentResult(sql, result)
        ├─ SQL'i QueryEditor'e koyar (setPlainText)
        └─ openQueryTab(sql, result)  ─► YENİ sonuç sekmesinde grid (aynı sonuç,
                                          tekrar sorgulanmaz)
   Guard: salt-okunur DEĞİLSE → yalnız editöre kopyalanır, çalıştırılmaz.
```

## Sürücü-Bağımsız Bağlantı Soyutlaması — M7-D

```
ConnectionDialog (driver combo + koşullu alanlar + QSettings profilleri)
        │  config() → core::ConnectionConfig{driverType, filePath | host/port/db/user/pass}
        ▼
DatabaseManager::connect(cfg) ── switch(driverType)
        ├─ QSQLITE  → setDatabaseName(filePath)            [fiilen bağlanır]
        └─ QPSQL/QMYSQL/QODBC → host/port/db/user/pass     [plugin takılırsa hazır]
QSqlDatabase::drivers() → availableDrivers() (combo'yu doldurur)
```

FIilen yalnız **SQLite** bağlanır; sunucu sürücüleri Qt SQL plugin'i kurulduğunda
seçilebilir hale gelir (çağrı noktaları değişmeden). MongoDB/Redis/ClickHouse
QtSql satır/sütun modeliyle bağdaşmadığı için kapsam dışıdır.

## SQL Editörü: Highlighting + Auto-complete + EXPLAIN + Format — M7-G

```
QueryEditor (QPlainTextEdit)
  ├─ ui::SqlHighlighter(document())   → keyword/string/sayı/yorum renklendirme
  ├─ QCompleter + QStringListModel    → tablo adları + table.column (refreshSchema besler)
  │     Ctrl+Space elle tetik · kelime/. otomatik · Enter/Tab/Esc popup yönlendirme
  ├─ formatSql()                      → keyword-newline düzenleme (Ctrl+Shift+F)
  └─ MainWindow::explainCurrentQuery  → DatabaseManager::explainQueryPlan(sql)
         └─ EXPLAIN QUERY PLAN sonucu ayrı sonuç sekmesinde (Ctrl+Shift+X)
```

## Neden Böyle Tasarlandı?

- **Performans:** native C++/Qt, thread yok (SQLite yerel, single-thread yeterli ve basit), grid `ResizeToContents` yerine `Interactive` sütun boyutlandırma (büyük sonuçlarda UI killenmesin), forward-only kapalı ama only-son-SELECT grid'e taşınır.
- **Dayanıklılık:** Qt exception yerine `QueryResult`/hata-iadesi; hiçbir hatalı SQL çökertmez (test ile kanıtlı).
- **Bağımlılık:** Qt6::Widgets + Qt6::Sql + Qt6::Charts + Qt6::Network + Qt6::Xml (XML export için `QXmlStreamWriter`). AI tamamen
  **offline**'dir: yalnızca `localhost`'taki yerel Ollama servisi (hazır değilse
  deterministik kural motoru) kullanılır; **bulut LLM / cihaz dışı ağ yok**. Chat
  sadece **salt-okunur SQL** üretir/çalıştırır (guard ile garanti).

## Bilinen Kısıtlar

- Tek aktif bağlantı (çoklu DB = Milestone 2).
- Grid düzenleme yalnızca **tablo-gözatma modunda** (ağaçtan seçim/çift tık);
  rastgele SQL sonuçları salt-okunurdur (SQLite view/aggregate write-back yok).
- ER layout otomatik grid (elle pozisyonlama/saklama yok).
- SQLite `ALTER COLUMN` desteklemez → kolon silme/değiştirme rebuild wizard'ı Milestone 4.

## Hızlı Komutlar

```bash
# derle
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
# çalıştır
./build/QuickDb.app/Contents/MacOS/QuickDb > build/quickdb.log 2>&1 &
# testler
./build/smoke_test
QT_QPA_PLATFORM=offscreen ./build/ui_test
./build/ai_test
./build/io_test
# canlı buton logu izleme
tail -f build/quickdb.log
```
