[![en](https://img.shields.io/badge/lang-en-red.svg)](https://github.com/baziorek/STC_editor/tree/master/README_ang.md)

# Edytor STC

Prosty edytor tekstu stworzony w Qt, ułatwiający wstawianie znaczników [STC](https://cpp0x.pl/kursy/Kurs-STC/169) używanych na platformie [cpp0x.pl](https://cpp0x.pl/).

## Opis
Edytor ten został zaprojektowany, aby uprościć pracę z językiem znaczników STC na potrzeby tworzenia treści dla [cpp0x.pl](https://cpp0x.pl/). Oferuje prosty interfejs oparty na Qt z polem tekstowym i przyciskami do wstawiania znaczników STC oraz dedykowane funkcje zwiększające produktywność.

![Zrzut ekranu](screens/applicationSample.png)

## Dedykowane funkcje
 1. **Kolorowanie składni**: Podświetlanie znaczników STC dla lepszej czytelności.
 2. **Weryfikacja zamknięcia znaczników**: Sprawdza, czy wszystkie znaczniki STC są poprawnie zamknięte.
 3. **Transformacja tekstu**: Zmiana zaznaczonego tekstu na małe litery, wielkie litery, camelCase na snake_case lub odwrotnie.
 4. **Podgląd kontekstu dokumentu**:
    - Wykrywa znaczniki (np. `[h1]Nagłówek[/h1]`), ich numery linii oraz zawartość (np. „Nagłówek”).
    - Umożliwia przejście do określonych pozycji w dokumencie na podstawie kontekstu.
    - Filtrowanie wybranych znaczników (np. tylko `[h1]`).
    - Śledzenie pozycji kursora w kontekście dokumentu w czasie rzeczywistym.
 5. **Podgląd obrazów**: Najedź myszą na `[img src="ścieżka/do/obrazu.png"]`, aby zobaczyć podgląd obrazu (wymaga prawidłowej ścieżki).
 6. **Usuwanie znaczników**: Kliknij prawym przyciskiem wewnątrz znaczników (np. `[b]Pogrubiony tekst[/b]`), aby usunąć znaczniki, pozostawiając tylko treść (np. `Pogrubiony tekst`).
 7. **Formatowanie kodu C++**: Kliknij prawym przyciskiem wewnątrz `[cpp]...[/cpp]`, aby sformatować kod za pomocą `clang-format` (wymaga zainstalowanego `clang-format`).
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

## Ogólne funkcje edytora
Jeśli ktoś chce tego używać do innych celów:
 1. **Operacje na plikach**: Wczytywanie i zapisywanie plików z obsługą kodowania UTF-8, przeładowanie pliku.
 2. **Ostatnio otwarte pliki**: Zapamiętuje ostatnio używane pliki wraz z ostatnią pozycją w pliku dla szybkiego dostępu.
    - Zapamiętana jest również data ostatniego otwarcia pliku w edytorze. 
 3. **Wykrywanie zmian zewnętrznych**: Powiadamia o modyfikacjach pliku z zewnątrz.
 4. **Numerowanie linii**: Dodawanie numeracji do zaznaczonych linii przez menu kontekstowe (funkcja z menu kontekstowego).
 5. **Łączenie linii**: Łączenie wielu zaznaczonych linii w jedną, oddzieloną spacjami (funkcja z menu kontekstowego).
 6. **Kopiowanie ścieżki pliku**: Kopiowanie nazwy pliku lub pełnej ścieżki do schowka.
 7. **Kontrola wcięć**: `Tab` przesuwa zaznaczony tekst w prawo, `Shift+Tab` w lewo.
 8. **Lista skrótów**: Dostępna z menu aplikacji.
 9. **Wyszukiwanie tekstu**:
    - Wyszukiwanie z opcjami uwzględniania wielkości liter i dopasowania całych słów.
    - Wyświetlanie liczby wystąpień:
      - Z uwzględnieniem wielkości liter.
      - Bez uwzględniania wielkości liter.
      - Z uwzględnieniem wielkości liter, tylko całe słowa.
      - Bez uwzględniania wielkości liter, tylko całe słowa.
10. **Skalowanie czcionki**: Powiększanie/pomniejszanie czcionki za pomocą `Ctrl++` i `Ctrl+-` lub `Ctrl+MOUSE_SCROLL`.
11. **Pasek stanu**: Pokazuje nazwę otwartego pliku, ale też liczbę niezapisanych zmienionych linii, czas ostatniej edycji i zapisu (tylko przy niezapisanych zmianach).
12. **Oznaczenie aktualnej linii**: Śledzenie aktualnej pozycji kursora klawiatury w ramach linijki

## Planowane funkcjonalności

### Przed pierwszym wydaniem

- **Funkcje**:
- Pokazywanie zmienionych linii:
    https://github.com/cubicdaiya/dtl używane przez https://github.com/qtpm/QDiffView ewentualnie to https://github.com/google/diff-match-patch https://stackoverflow.com/questions/47375116/how-to-use-google-diff-match-patch-library-in-c-qt
- Wyszukaj i zamień: `Ctrl+R` z możliwością wyłączenia poszczególnych wykrytych pozycji.
  - Sprawdzania:
     - Sprawdzanie, czy znaczniki `[run]` znajdują się wewnątrz `[pkt]`.
     - Weryfikacja, czy wszystkie znaczniki są zamknięte (np. po opuszczeniu linijki sprawdzamy czy są tam zmiany, jak tak, to czy jest tam nowy tag)
     - Weryfikacja odpowiednich atrybutów w tagach (czy w cudzysłowiu, czy tylko dozwole atrubytu)
     - Weryfikacja czy wprowadzono tylko legalne tagi STC
- Dodatkowe eventy z edytora: zmieniona linia i reakcja na nie różnych przeglądających koncepty
- MiniBug: funkcjonalność zmiany wielkości czcionki zakłóca nowe tagi. Jak powiększymy czcionkę i potem dodamy coś np. H1, to on będzie miał czcionkę nawet mniejszą niż reszta, mimo iż to nagłówek

### Pomysły na przyszłość

1. Obsługa zakładek do szybkiego przechodzenia do miejsc w kodzie.
2. Skróty `Alt+Lewo` i `Alt+Prawo` do nawigacji wstecz/dalej po pozycjach w kodzie.
3. Otwieranie wielu plików jednocześnie.
4. Widok sąsiadujący do porównywania plików.
5. Eksport bloków kodu do osobnych plików.
6. Konsolidacja obrazów do jednego katalogu z aktualizacją ścieżek w znacznikach STC.
7. Integracja analizatora składni C++ (np. [flex](https://github.com/westes/flex)).
8. Podświetlanie składni C++ i Pythona za pomocą [QCXXHighlighter](https://github.com/Megaxela/QCodeEditor) (licencja MIT).
9. Kreator tabel dla znaczników STC.
10. Wyświetlanie statystyk zmian w czasie rzeczywistym (dodane, zmodyfikowane, usunięte linie).
11. Zamiana prefiksów adresów URL dla obrazów na serwerze.
12. Historia wprowadzanych zmian (`Ctrl+Z`).
13. Wyszukiwanie wielu słów w tej samej linii niezależnie od kolejności.
14. Sprawdzanie pisowni po polsku (np. [nuspell](https://github.com/nuspell/nuspell) lub [spellchecker Qt](https://doc.qt.io/qt-6/qtwebengine-webenginewidgets-spellchecker-example.html)). https://forum.qt.io/topic/158878/check-spelling-in-qlineedit-and-qplaintextedit
15. Obsługa różnych kodowań plików z automatycznym rozpoznawaniem.
16. Obsługa wtyczek, być może z użyciem Lua.
17. Integracja dokumentacji cppreference (jak w `cppman` lub QtCreator).
18. Nagrywanie i odtwarzanie makr.
19. Zastąpienie listy kontekstu widżetem drzewiastym.
20. Dodanie ikon do akcji w menu i menu kontekstowym.
21. Pokazywanie różnic w liniach znak po znaku.
22. Optymalizacja wydajności edytora przy szybkim pisaniu.
23. Dopasowanie rozmiaru numeracji linii do wielkości czcionki.
24. Skanowanie dokumentu w osobnym wątku dla lepszej wydajności.
25. Podświetlanie bieżącej linii dla lepszej widoczności kursora.
26. Sensowne funkcjonalności z innych podobnych edytorów np. [Scribe-Text-Editor](https://github.com/AleksandrHovhannisyan/Scribe-Text-Editor)
27. Ctrl + F gdy mamy focus w dokumencie powinno zyskać focus, a nie zniknąć
28. Sprawdzenie czy link istnieje
29. Następna zmiana: przycisk pozwalający skakać po zmianach w dokumencie
30. chowanie breadcrumb
31. enter na wyszukiwaniu skacze do miejsca wykrycia
32. SHIFT N: kolejne wykrycie i też trzeba by wymyślić poprzednie
33. Dodać check: czy nie zamykam nieotwartego tagu?
34. Sprawdzenie czy Run nie jest poza csv lub pkt
35. Refaktoring: Jeśli wczytywanie pliku jest przez CodeEditor, to zapis pliku też konsekwentnie powinien stamtąd iść
36. Tłumaczenia aplikacji na wiele języków: QLinguist
37. Rozważyć użycie innej biblioteki do porównywania np. https://github.com/google/diff-match-patch
38. Szukanie tylko w kodzie
39. Przeciągnij i upuść pliku z kodem
40. Formatowanie kodu C++ dedykowane
41. W linijce gradient ostatnio używanych linii z numerami ile temu
42. Przy CTRL + SCROLL wyświetlanie informacji o rozmiarze czcionki (jak QtCreator)
43. Ukrycie tagów, nie licząc otaczających (rich text editor)
44. Przycisk na sformatowanym tekście powinien usunąć dane formatowanie: np. jak klikniemy CTRL+B na pogrubionym to aby nie był pogrubiony
45. Wyszukiwanie tekstu może też oznaczyć szukany tekst w dokumencie np. ramką
46. Kontekst trzeba by uprościć, aby zawierał tylko nagłówki (domyślnie)
47. Historia zmian w linii
48. Precompiled headers dla codeEditor.h i moduły
49. IWYU podpiąć pod CMake'a
50. Zwijanie nagłówków i kodu (jak funkcje w środowiskach programistycznych)
51. Automatyczne backupy treści
52. Do formatowania znajduje plik `clang-format`, który jest w aktualnym katalogu (obok pliku, który edytujemy)
53. W danej linii da się wpisać dane np.: `QTextBlockUserData` i potem `block.setUserData(data);` - może da się to wykorzystać w optymalizacji
54. Kurs STC wbudowany w program
55. Prawy przycisk myszy na: tagach `img`, `a href`, `pkt`, `csv` powinien dawać możliwość dodania/usunięcia atrybutów
56. Śledzenie pozycji między pozycją w dokumencie źródłowym a podglądem HTML
57. Podgląd terminala
58. Wklejenie linka na zaznaczony tekst powinno otoczyć tekst przez `[a href=`
59. Ctrl + V gdy mamy obrazek
60. Ctrl + V gdy mamy tabelkę
61. Podgląd obrazka po najechaniu - nawet gdy adres internetowy
62. Brakuje przycisku wstawiającego img
63. Zmiana wielkości czcionki powinna też wpłynąć na czcionkę w ramach numeracji linii
64. Podgląd strony internetowej po najechaniu myszką
65. Popraw numeracje dla zaznaczonego tekstu

## Współpraca

Zapraszam do współpracy! Propozycje zmian i pull requesty są mile widziane, aby uczynić ten edytor jeszcze bardziej użytecznym.

### Znalezione bugi
Bardzo proszę o dokładne informacje jakie to bugi, czyli:
1. W jakiej sytuacji (Sekwencja kroków)
2. Kod STC, który to spowodował
3. Jakie jest oczekiwane zachowanie wg Ciebie?

## Przydatność

Narzędzie to okazało się bardzo pomocne przy tworzeniu artykułów na cpp0x.pl, np. [porównującym python i C++](https://cpp0x.pl/artykuly/Inne-artykuly/Porownanie-C++-i-Python-roznice-w-skladni-i-podejsciu-programistycznym/99). Ułatwia wstawianie znaczników STC i debugowanie niezamkniętych znaczników.

## Przydatne linki

- [Interpreter STC on-line](https://cpp0x.pl/stc/)
- [Źródło użytych ikonek: MDI](https://pictogrammers.com/library/mdi/)

## Ostrzeżenie

Edytor jest prostym narzędziem i nie został gruntownie przetestowany pod kątem niezawodności. Używaj go na własne ryzyko, ponieważ może nie zachować ważnych treści. Jak dotąd jednak nie zawiódł autora.
