[![en](https://img.shields.io/badge/lang-en-red.svg)](https://github.com/baziorek/STC_editor/tree/master/README_ang.md)
![Build Status: Linux](https://github.com/baziorek/STC_editor/actions/workflows/release-ubuntu22.04.yml/badge.svg)
![Build Status: Windows](https://github.com/baziorek/STC_editor/actions/workflows/release-windows.yml/badge.svg)
![Building & running tests](https://github.com/baziorek/STC_editor/actions/workflows/test-linux.yml/badge.svg)


<a href="https://pl.wikipedia.org/wiki/Totus_Tuus"><img width="10%"  alt="Totus Tuus" src="https://github.com/user-attachments/assets/02c77754-5d69-4688-b99b-496995de4cfe" /></a>

# Edytor STC

Prosty edytor tekstu stworzony w Qt, ułatwiający wstawianie znaczników [STC](https://cpp0x.pl/kursy/Kurs-STC/169) używanych na platformie [Cpp0x.pl](https://cpp0x.pl/).

## Opis
Edytor ten został zaprojektowany, aby uprościć pracę z językiem znaczników STC na potrzeby tworzenia treści dla [cpp0x.pl](https://cpp0x.pl/). Oferuje prosty interfejs oparty na Qt z polem tekstowym i przyciskami do wstawiania znaczników STC oraz dedykowane funkcje zwiększające produktywność.

![Zrzut ekranu](screens/applicationSample.png)

## Dedykowane funkcje
Jak ktoś chce tego używać na potrzeby pisania na stronę [Cpp0x.pl](https://cpp0x.pl/) (do czego zachęcam):
 1. **Kolorowanie składni**: Podświetlanie znaczników STC dla lepszej czytelności.
 2. **Weryfikacja zamknięcia znaczników**: Sprawdza, czy wszystkie znaczniki STC są poprawnie zamknięte.
 3. **Transformacja tekstu**: Zmiana zaznaczonego tekstu na małe litery, wielkie litery, camelCase na snake_case lub odwrotnie.
 4. **Podgląd kontekstu dokumentu**:
    - Wykrywa znaczniki (np. `[h1]Nagłówek[/h1]`), ich numery linii oraz zawartość (np. „Nagłówek”).
    - Umożliwia przejście do określonych pozycji w dokumencie na podstawie kontekstu.
    - Filtrowanie wybranych znaczników (np. tylko `[h1]`).
    - Śledzenie pozycji kursora w kontekście dokumentu w czasie rzeczywistym.
 5. **Podgląd obrazów**: Najedź myszą na `[img src="ścieżka/do/obrazu.png"]`, aby zobaczyć podgląd obrazu (wymaga prawidłowej ścieżki).
    - Podgląd również obrazków z internetu
 6. **Usuwanie znaczników**: Kliknij prawym przyciskiem wewnątrz znaczników (np. `[b]Pogrubiony tekst[/b]`), aby usunąć znaczniki, pozostawiając tylko treść (np. `Pogrubiony tekst`).
 7. **Formatowanie kodu C++**: Kliknij prawym przyciskiem wewnątrz `[cpp]...[/cpp]`, aby sformatować kod za pomocą `clang-format` (wymaga zainstalowanego `clang-format`).
    - Jeśli obok pliku tekstowego znajduje się plik z ustawieniami formatowania ".clang-format" to zostanie on użyty, w przeciwny wypadku domyślne użyte zostanie "LLVM".
 8. **Kompilacja kodu C++**: Kliknij prawym przyciskiem wewnątrz `[cpp]...[/cpp]`, aby skompilować kod za pomocą `g++` (wymaga zainstalowanego `g++`).
 9. **Statystyki pliku**: Wyświetla statystyki specyficzne dla STC, np. użycie znaczników, obok standardowych metryk edytora.
10. **Nawigacja okruszkowa**: Dynamicznie aktualizowany pasek adresu pokazujący bieżącą pozycję w strukturze dokumentu STC, z możliwością kliknięcia.
11. **Śledzenie zmian**: Śledzi zmienione linie za pomocą biblioteki pydifflib-cpp.
12. **Listowanie kodów w pliku**: Oddzielny widget, który śledzi na bieżąco pozycje kodów `[cpp]` i innych.
13. **Dedykowane przeciągnij i upuść**: Do aplikacji można przeciągać pliki i zostaną odpowiednio obsłużone:
    - Ścieżka do plików graficznych zostanie otoczona tagami `[img src="ścieżka/do/przeciagnietego/obrazu.png"]`
    - Zawartość plików tekstowych o rozszerzeniach wskazujących na C/C++ zostanie wstawione i otoczona tagami `[cpp]...[/cpp]`
    - Zawartość pozostałych plików tekstowych zostanie wstawiona i otoczona tagami `[code]...[/code]`
14. **Podgląd strony w czasie rzeczywistym**: Jest możliwe użycie w programie [backendu konwertującego kod ze znacznikami STC na kod HTML](https://cpp0x.pl/stc/).
    - Dostępne są również statystyki: ile danych wysłano i pobrano, widoczne po najechaniu myszką na obszar renderujący
15. **Śledzenie TODO w dokumencie**: W podglądzie dokumentu widać komentarze `TODO: ` wraz z tekstem na prawo od tego, są one klikalne i wtedy przechodzą do danej pozycji.
16. **Klikalne linki**: wewnątrz tagów z linkiem `[a href="..."]` lub `[a href="..." name="..."]` możemy przytrzymać CTRL + lewy przycisk myszy i nam się otworzy dany link
17. **Wklejane linki wklejane w odpowiedni sposób**: jak wklejamy przez CTRL+V, a w schowku jest link to otacza linka tagami linka `[a href="link"]`
    - Po chwili zostanie pobrana z linka i dodana jako kolejny atrybut nazwa strony, czyli zamiast `[a href="link"]` będzie `[a href="link" name="Nazwa strony dla linka"]`
    - Jeśli jest zaznaczony tekst to "podlinkowuje go" przez otoczenie: `[a href="link" name="Zaznaczony wcześniej tekst"]`
18. **Wklejalne tabelki**: gdy w schowku jest tabelka to jest ona otaczana odpowiednimi tagami `[csv]...[/csv]` a komórki są rozdzielane średnikami.
    - Jeśli wklejony tekst zawiera tagi, to odpowiednie komórki są otaczane przez `[run]`, a typ tabelki się zmienia na `[csv ext]`
19. **Wklejanie rich-text**: jak wkleimy tekst ze strony lub np. programu Office to HTML jest zamieniany na znaczniki STC (częściowo), m.in. linki, pogrubienia itp.
20. **Inteligentne przyciski**: Niektóre przyciski mają duże ułatwienie działania:
    - Przycisk `a href` dostosowuje się do zaznaczonego tekstu, jak go nie ma to po prostu wstawia pusty tag, ale jak coś jest zaznaczone to próbuje wykryć tam link i odpowiednio otoczyć tagiem dopasowując zarówno linka jak i nazwę linka
    - Przycisk `img` dostosowuje się do zaznaczonego tekstu, jak go nie ma to po prostu wstawia pusty tag, ale jak coś jest zaznaczone to próbuje wykryć ścieżkę do pliku graficznego i odpowiednio otoczyć tagiem dopasowując zarówno źródło obrazka jak i opis
21. **Dodawanie/usuwanie atrybutów dla tagów z menu kontekstowego**: Gdy klikniemy prawym przyciskiem myszy na tagu, które ma możliwe różne atrybuty, to pojawia się możliwość wyklikania opcjonalnych atrybutów do dodania/usunięcia.

## Ogólne funkcje edytora
Edytor też nadaje sie do innych celów - do ogólnej edycji dokumentu, oto wybrane funkcjonalności, których brakuje mi w zwykłych edytorach:
 1. **Operacje na plikach**: Wczytywanie, zapisywanie i przeładowanie pliku.
 2. **Ostatnio otwarte pliki**: Zapamiętuje ostatnio używan\e pliki wraz z ostatnią pozycją w pliku dla szybkiego dostępu.
    - Zapamiętana jest również data ostatniego otwarcia pliku w edytorze. 
 3. **Wykrywanie zmian zewnętrznych**: Powiadamia o modyfikacjach pliku z zewnątrz.
 4. **Inteligentne menu kontekstowe dla tekstu**, m.in.:
    - **Numerowanie linii**: Dodawanie numeracji do zaznaczonych linii przez menu kontekstowe (funkcja z menu kontekstowego).
    - **Dodawania punktów do każdej linii zaznaczonego tekstu**:
    - **Sortowanie zaznaczonych linijek**: z pominięciem wielkości znaków
    - **Łączenie linii**: Łączenie wielu zaznaczonych linii w jedną, oddzieloną spacjami (funkcja z menu kontekstowego).
    - **Konwersje między wielkością znaków zaznaczonego tekstu**: na małe, na duże, CamelCase <-> snake_case
 5. **Kopiowanie ścieżki pliku**: Kopiowanie nazwy pliku lub pełnej ścieżki do schowka.
 6. **Kontrola wcięć**: `Tab` przesuwa zaznaczony tekst w prawo, `Shift+Tab` w lewo.
 7. **Lista skrótów**: Dostępna z menu aplikacji.
 8. **Wyszukiwanie tekstu**:
    - Wyszukiwanie z opcjami uwzględniania wielkości liter i dopasowania całych słów.
    - Wyświetlanie liczby wystąpień:
      - Z uwzględnieniem wielkości liter.
      - Bez uwzględniania wielkości liter.
      - Z uwzględnieniem wielkości liter, tylko całe słowa.
      - Bez uwzględniania wielkości liter, tylko całe słowa.
    - ENTER i SHIFT+ENTER odpowiednio przesuwają do kolejnego/poprzedniego wystąpienia z tabeli
    - Strzałki dół/góra dzialają jak powyższe, ale nie przeskakują z focusem do edytora tekstu
 9. **Skalowanie czcionki**: Powiększanie/pomniejszanie czcionki za pomocą `Ctrl++` i `Ctrl+-` lub `Ctrl+MOUSE_SCROLL`.
10. **Pasek stanu**: Pokazuje nazwę otwartego pliku, ale też liczbę niezapisanych zmienionych linii, czas ostatniej edycji i zapisu (tylko przy niezapisanych zmianach).
11. **Oznaczenie aktualnej linii**: Śledzenie aktualnej pozycji kursora klawiatury w ramach linijki
12. **Szczegółowy wykaz niezapisanych zmian**: Gdy mamy niezapisane zmiany i próbujemy wyjść z programu to poza spytaniem użytkownika czy na pewno chce wyjść bez zapisania zmian pojawia się od razu diff w formie tabeli pokazujący różnice w odpowiadających sobie liniach.
    - Możliwść przywrócenia zmian względem danej linii z dysku.
    - Zmiany mają również pokazane różnicę w numerach poszczególnych znaków, wtedy możemy lepiej odróżnić znaki, które "na oko" są takie same.
13. **Ukrywalne widgety**: wszystko poza edytorem tekstu można ukryć, dzięki temu pewne funkcje są wyłączone i edytowanie działa szybciej.
14. **Obsługa różnych kodowań pliku tekstowego**: Nie tylko UTF-8. Jest to dzięki bibliotece [uchardet](https://gitlab.freedesktop.org/uchardet/uchardet).

## ⬇️ Pobieranie (najnowsza wersja zbudowana automatycznie)

Możesz pobrać najnowszą skompilowaną wersję **STC_editor**, z ostatniego poprawnie zakończonego procesu budowania przez GitHub Actions (wymagane konto na Github - tak oni wymyślili):

👉 [⬇️ Pobierz wersję skompilowaną w formacie AppImage (działa na systemie Linux)](https://github.com/baziorek/STC_editor/actions/workflows/release-ubuntu22.04.yml?query=branch%3Amaster)
👉 [⬇️ Pobierz wersję skompilowaną na Windowsa](https://github.com/baziorek/STC_editor/actions/workflows/release-windows.yml?query=branch%3Amaster)

> Po przejściu pod powyższy link:
> 1. Kliknij na najnowsze zakończone zadanie (workflow).
> 2. Przewiń stronę na dół.
> 3. Pobierz artefakt o nazwie `STC_editor-x86_64.AppImage` lub `STC_editor-win64.zip`

📦 [⬇️ Pobierz najnowszą wersję release](https://github.com/baziorek/STC_editor/releases/latest)  
*Aktualnie brak opublikowanych wydań (release). Gdy się pojawią — będą dostępne tutaj.*


## Planowane funkcjonalności

1. Wyszukiwanie wielu słów w tej samej linii niezależnie od kolejności.
2. Wyszukaj i zamień: `Ctrl+R` z możliwością wyłączenia poszczególnych wykrytych pozycji.
3. Integracja analizatora składni C++ (np. [flex](https://github.com/westes/flex)).
4. Podpięcie sztucznej inteligencji np. [Ollama](https://ollama.com)
5. Podświetlanie składni C++ i Pythona za pomocą [QCXXHighlighter](https://github.com/Megaxela/QCodeEditor) (licencja MIT).
6. Kreator tabel dla znaczników STC.
7. Wyświetlanie statystyk zmian w czasie rzeczywistym (liczba linii, znaków, rozmiar pliku, linia i kolumna).
8. Historia wprowadzanych zmian (`Ctrl+Z`).
9. Sprawdzanie pisowni po polsku (np. [nuspell](https://github.com/nuspell/nuspell) lub [spellchecker Qt](https://doc.qt.io/qt-6/qtwebengine-webenginewidgets-spellchecker-example.html)). https://forum.qt.io/topic/158878/check-spelling-in-qlineedit-and-qplaintextedit
10. Dodać stoper aktywności
11. Integracja dokumentacji cppreference (jak w `cppman` lub QtCreator).
12. Dopasowanie rozmiaru numeracji linii do wielkości czcionki.
13. Zamiana warunkowa -np. zamień jeśli już nie jest. Np. jak chcę zamienić `cout` na `std::cout` to zamieni tylko jeśli nie jest to `std::cout`
14. Akcja: Popraw numeracje dla zaznaczonego tekstu
15. Sprawdzenie czy link istnieje
16. MiniBug: funkcjonalność zmiany wielkości czcionki zakłóca nowe tagi. Jak powiększymy czcionkę i potem dodamy coś np. H1, to on będzie miał czcionkę nawet mniejszą niż reszta, mimo iż to nagłówek
17. FindWidget - aby aktualizował pozycje w tekście na bieżąco przy dodawaniu/usuwaniu linii.
18. Dodać check: czy nie zamykam nieotwartego tagu?
19. Sprawdzenie czy Run nie jest poza csv lub pkt
20. Ctrl + V gdy mamy obrazek w schowku - wtedy powinno zaproponować umieszczenie obrazka w odpowiednim katalogu
21. Tłumaczenia aplikacji na wiele języków: QLinguist
22. Rozważyć użycie tej samej biblioteki do porównywania zarówno linii jak i znaków np. https://github.com/google/diff-match-patch
23. Formatowanie kodu C++ dedykowane
24. Przy CTRL + SCROLL wyświetlanie informacji o rozmiarze czcionki (jak QtCreator)
25. Przycisk na sformatowanym tekście powinien usunąć dane formatowanie: np. jak klikniemy CTRL+B na pogrubionym to aby nie był pogrubiony
26. Wyszukiwanie tekstu może też oznaczyć szukany tekst w dokumencie np. ramką
27. Śledzenie pozycji między pozycją w dokumencie źródłowym a podglądem HTML
28. Historia zmian w linii
29. Precompiled headers dla codeEditor.h i moduły
30. IWYU podpiąć pod CMake'a
31. Zwijanie nagłówków i kodu (jak funkcje w środowiskach programistycznych)
32. Automatyczne backupy treści
33. W danej linii da się wpisać dane np.: `QTextBlockUserData` i potem `block.setUserData(data);` - może da się to wykorzystać w optymalizacji
34. Kurs STC wbudowany w program
35. Optymalizacja wydajności edytora przy szybkim pisaniu.
36. Sprawdzania:
     - Sprawdzanie, czy znaczniki `[run]` znajdują się wewnątrz `[pkt]`.
     - Weryfikacja, czy wszystkie znaczniki są zamknięte (np. po opuszczeniu linijki sprawdzamy czy są tam zmiany, jak tak, to czy jest tam nowy tag)
     - Weryfikacja odpowiednich atrybutów w tagach (czy w cudzysłowiu, czy tylko dozwole atrubytu)
     - Weryfikacja czy wprowadzono tylko legalne tagi STC
37. MiniBug - gdy zaznaczony tekst poprzedzimy numeracją linii (funkcja z menu kontekstowego) to zaznaczenie zostaje, ale jest zaznaczone za nr 1. a powinno obejmować również nr 1.
38. Szukanie tylko w kodzie
39. Podgląd terminala np. przez https://github.com/lxqt/qtermwidget - tylko czy mi to nie zrobi GPLa?
40. Zastąpienie listy kontekstu widżetem drzewiastym.
41. W linijce gradient ostatnio używanych linii z numerami ile temu
42. Ukrycie tagów, nie licząc otaczających (rich text editor)
43. Możliwość wyłączenia poszczególnych aspektów kolorowania składni.
44. Następna/poprzednia zmiana: przycisk pozwalający skakać po zmianach w dokumencie
45. Zmiana wielkości czcionki powinna też wpłynąć na czcionkę w ramach numeracji linii
46. Sensowne funkcjonalności z innych podobnych edytorów np. [Scribe-Text-Editor](https://github.com/AleksandrHovhannisyan/Scribe-Text-Editor)
47. Podgląd strony internetowej po najechaniu myszką
48. Wsparcie dla MD na bazie: https://github.com/Qt-Widgets/notes
49. Inne widgety np. https://github.com/Qt-Widgets/SlidingStackedWidget-1 z listy: https://github.com/Qt-Widgets/ lub https://qwt.sourceforge.io/index.html
50. Skanowanie dokumentu w osobnym wątku dla lepszej wydajności.
51. Nagrywanie i odtwarzanie makr.
52. Obsługa wtyczek, być może z użyciem Lua.
53. Zamiana prefiksów adresów URL dla obrazów na serwerze.
54. Dyktowanie tekstu (biblioteka [Whisper](https://github.com/openai/whisper))
55. Skróty `Alt+Lewo` i `Alt+Prawo` do nawigacji wstecz/dalej po pozycjach w kodzie.
56. Otwieranie wielu plików jednocześnie.
57. Widok sąsiadujący do porównywania plików.
58. Eksport bloków kodu do osobnych plików.
59. Konsolidacja obrazów do jednego katalogu z aktualizacją ścieżek w znacznikach STC.
60. Zapis poszczególnych linii na dysk przy porównywaniu diffa.
61. Przemianuj otwarty plik
62. Może AI mi podzieli CodeEditor aby nie był GodObject

## Współpraca

Zapraszam do współpracy! Propozycje zmian i pull requesty są mile widziane, aby uczynić ten edytor jeszcze bardziej użytecznym.

### Znalezione bugi
Bardzo proszę o dokładne informacje jakie to bugi, czyli:
1. W jakiej sytuacji (Sekwencja kroków)
2. Kod STC, który to spowodował
3. Jakie jest oczekiwane zachowanie wg Ciebie?

## Przydatność

Narzędzie to okazało się bardzo pomocne przy tworzeniu artykułów na [Cpp0x.pl](https://cpp0x.pl/), np. [porównującym python i C++](https://cpp0x.pl/artykuly/Inne-artykuly/Porownanie-C++-i-Python-roznice-w-skladni-i-podejsciu-programistycznym/99). Ułatwia wstawianie znaczników STC i debugowanie niezamkniętych znaczników.

## Przydatne linki

- [Interpreter STC on-line](https://cpp0x.pl/stc/)
- [Źródło użytych ikonek: MDI](https://pictogrammers.com/library/mdi/)

## Używane biblioteki zewnętrzne
1. [pydifflib-cpp](https://github.com/dominicprice/pydifflib-cpp) - do znajdywania różnic między liniami: które linie dodane, usunięte, zmodyfikowane. Licencja "PSF"
2. [diff-match-patch-cpp-stl](https://github.com/leutloff/diff-match-patch-cpp-stl/) - do znajdywania różnic między znakami w ramach odpowiadających sobie linijkach. Licencja "Apache 2.0"
3. [uchardet](https://gitlab.freedesktop.org/uchardet/uchardet) - biblioteka obsługujące różne rodzaje kodowań plików (nie tylko UTF-8). Licencja "MOZILLA PUBLIC LICENSE"

## Ostrzeżenie

Edytor jest prostym narzędziem i nie został gruntownie przetestowany pod kątem niezawodności. Używaj go na własne ryzyko, ponieważ może nie zachować ważnych treści. Jak dotąd jednak nie zawiódł autora.
