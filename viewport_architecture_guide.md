# Arsitektur Viewport di LibreCAD

Fitur **Viewport pada Tab Layout** di LibreCAD menggunakan arsitektur OOP (Object-Oriented Programming) yang memisahkan tanggung jawab ke dalam beberapa modul utama: **Data Model (Entitas)**, **Tampilan (View & Renderer)**, dan **Aksi (Interaksi User)**.

Berikut adalah panduan lengkap alur kerja dan abstraksi kelas yang terlibat:

---

## 1. Model Data (Entity Base)
Kelas-kelas ini bertanggung jawab untuk menyimpan informasi bentuk fisik, lokasi, dan properti matematis sebuah Viewport sebagai bagian dari gambar (tersimpan di dalam `Block` bernama `*Paper_Space`).

* **`RS_Entity`**  
  *(Abstraksi Paling Dasar)*  
  Merupakan basis dari seluruh objek gambar di LibreCAD. Memiliki kemampuan dasar seperti operasi *Undo/Redo* (`isUndone`), Seleksi (`isSelected`), Visibilitas, *Bounding Box*, dan pewarnaan (Pen).
  
  * **`RS_AtomicEntity`**  
    Turunan dari `RS_Entity` untuk entitas yang tidak memiliki "anak" (bukan *container*).

    * **`LC_Viewport`** `(lc_viewport.h / .cpp)`  
      Turunan langsung dari `RS_AtomicEntity` yang dikhususkan sebagai *Viewport*.
      * **Fungsi Utama:** Menyimpan objek `LC_ViewportData`, menentukan *bounding box* viewport, dan menggambar kotak batas luarnya (garis pinggir viewport) melalui fungsi `draw()`.
      * **Data:** Mengandung informasi apakah ia sedang aktif (`m_isActive`), dan menyimpan referensi ke `Model Space` (`m_modelGraphic`).

* **`LC_ViewportData`**  
  Struct ringan pembawa informasi esensial:
  * Titik diagonal batas viewport (`corner1`, `corner2`).
  * Titik tengah area yang disorot pada *Model Space* (`modelCenter`).
  * Skala perbesaran objek model di dalam viewport (`modelScale`).

---

## 2. Manajemen Antarmuka & Layar (View)
Bagian ini mengatur input dari pengguna (Mouse/Keyboard) dan menerjemahkan interaksi di atas kanvas.

* **`RS_GraphicView`** & **`QG_GraphicView`**  
  *(Abstraksi View & Adaptor Qt)*  
  Mengurus fondasi area gambar interaktif. `QG_GraphicView` merupakan implementasi antarmuka untuk framework Qt.

  * **`LC_LayoutView`** `(lc_layoutview.h / .cpp)`  
    Kelas turunan utama untuk Tab Layout (Paper Space).
    * **Fungsi Utama:** 
      * Menyediakan wadah `*Paper_Space` bagi entitas-entitas layout.
      * Menyimpan *pointer* ke viewport yang sedang diaktifkan (`m_activeViewport`).
      * Membelokkan (*intercept*) *mouse events* (klik, *drag*, *scroll*) dan mentransformasi koordinatnya ke dalam *Model Space* (`executeWithModelSpaceTransform`) jika sebuah viewport sedang aktif.
      * Mencegah event berjalan pada viewport yang telah dihapus menggunakan pengecekan `!isUndone()`.

* **`LC_GraphicViewport`** `(lc_graphicviewport.h)`  
  Objek yang menghitung matematika dasar untuk mentransformasi koordinat nyata (*World Coordinates*) ke koordinat layar (*GUI / Pixel Coordinates*). Menyimpan informasi `Offset` dan `Factor` layar.

* **`LC_ViewportZoomDelegate`**  
  *Interface* (*Delegate*) khusus yang memungkinkan `LC_GraphicViewport` memberikan sinyal ke `LC_LayoutView` saat aksi Zoom/Pan sedang terjadi.

---

## 3. Render Engine (Penggambar)
Sistem ini membaca *Model Data* dan mencetaknya ke layar melalui instruksi visual (garis, warna, kliping area).

* **`RS_Painter`**  
  *(Abstraksi Pelukis)*  
  Bekerja secara independen dari framework, menyediakan alat-alat perintah lukis matematis seperti `drawLineWCS()` atau `setClipRect()`.

  * **`LC_WidgetViewPortRenderer`**  
    Penerjemah entitas ke layar (mempersiapkan *layer* standar, sumbu kursor, dll).

    * **`LC_LayoutViewRenderer`** `(lc_layoutviewrenderer.h / .cpp)`  
      Bertugas khusus merender Layout Viewport.
      * **Fungsi Utama:** 
        * Menggambar latar belakang *Paper Space* putih beserta bayangan (`drawPaper`).
        * Mencari seluruh entitas bertipe `LC_Viewport` di dalam `Paper Space`.
        * **Kliping Ajaib (`drawLayerEntitiesOver`):** Untuk setiap `LC_Viewport` valid, *renderer* ini menggunakan `painter->setClipRect()` agar gambar tidak keluar kotak. Lalu ia menghitung kalkulasi offset, dan memanggil `renderEntity()` untuk menggambar seluruh isi `Model Space` secara dinamik ke dalam area kliping kotak viewport tersebut.

---

## 4. Interaksi Objek (Action & State Machine)
Bagian ini mengatur *State Machine* ketika pengguna mencoba membuat *Viewport* baru dengan menggunakan *tool* yang ada.

* **`RS_ActionInterface`** & **`RS_PreviewActionInterface`**  
  *(Abstraksi Alat)*  
  Fondasi dari seluruh aksi perkakas (*tool action*) di LibreCAD (memiliki status klik 1, klik 2, previeu, dll).

  * **`LC_ActionDrawViewport`** `(lc_actiondrawviewport.h / .cpp)`  
    State machine pembuat viewport.
    * **State 1 (`SetCorner1`):** Menunggu klik titik awal kotak viewport.
    * **State 2 (`SetCorner2`):** Menggambar garis previeu putus-putus (*rubber band*) mengikuti pergerakan kursor mouse hingga diklik titik kedua.
    * **Trigger:** Ketika selesai, membuat entitas kelas `LC_Viewport` baru, memasukkannya ke *undo cycle*, dan menempatkannya ke dalam `Container` kertas (`m_paperSpace`).

---

### Alur Kerja Ringkas
1. **Membuat:** User memilih alat pembuat -> `LC_ActionDrawViewport` mencatat koordinat -> Entitas `LC_Viewport` (beserta `LC_ViewportData`) dibuat di *Container* Tab Layout.
2. **Merender:** Saat siklus *paint* layar berjalan, `LC_LayoutViewRenderer` melihat entitas tersebut, menggambar bingkai luarnya (lewat `LC_Viewport::draw`), menset batas area kliping di layar, lalu menggambar seluruh entitas `Model Space` (*Line, Circle*, dll) secara transparan menembus kotak tersebut.
3. **Mengaktifkan:** Jika di-klik ganda, `LC_LayoutView` menangkap sinyal mouse dan merubah status viewport menjadi `active`. Sejak saat itu, segala tarikan mouse (*Pan*) atau gulir roda mouse (*Zoom*) diteruskan (*forwarded*) untuk memanipulasi koordinat transformasi yang ada di `LC_ViewportData`, membuat objek *Model Space* tampak bergerak-gerak di dalam jendelanya.
