# STC_editor
Prosty edytor tekstu w QT ułatwiający wstawianie znaczników [STC](https://cpp0x.pl/kursy/Kurs-STC/169) używanych przez serwis [cpp0x.pl](https://cpp0x.pl/)

## Prostota
To nie jest bardzo zaawansowane narzędzie, jest to po prostu widget w QT z polem tekstowym, oraz przyciskami umożliwiającymi wstawianie wybranych tagów języka.
![image](https://github.com/user-attachments/assets/17571fce-a89c-4c99-a38a-fd72ee65c7d5)

### Funkcjonalności dedykowane
1. Kolorowanie składni
2. Sprawdzanie czy zamknięto wszystkie tagi
3. Możliwość zmiany zaznaczonego tekstu: na małe, na duże, camelCase -> snake_case i odwrotnie
4. Podgląd kontekstu dokumentu (przykładowo dla `[h1]Nagłówek[/h1]` będzie wykryty tag "h1", linia, oraz zawartość "Nagłówek"), a także:
    - możliwość skakania do danej pozycji w dokumencie po kontekście
    - możliwość filtrowania tylko wybranych tagów (np. tylko nagłówki `[h1]`)
    - na bieżąco śledzenie w której pozycji w kontekście znajduje się kursor tekstu
5. Podgląd obrazków po najechaniu myszką (podgląd dla `[img src="istniejąca/ścieżka/obrazek.png"]`).
6. Gdy wewnątrz tagów klikniemy myszką to pojawi się w menu kontekstowym opcja usunięcia tagu (przykładowo mamy: `[b]Pogrubiony tekst[/b]` to pojawi się opcja usunięcia `[b]` i wtedy będzie jedynie `Pogrubiony tekst`).
7. Gdy klikniemy wewnątrz tagów z kodem C++ (`[cpp]...[/cpp]`) to mamy opcje formatowania kodu przez `clang-format` (uwaga: musi on być zainstalowany w systemie)
8. Gdy klikniemy wewnątrz tagów z kodem C++ (`[cpp]...[/cpp]`) to mamy opcje kompilacji kodu przez `g++` (uwaga: musi on być zainstalowany w systemie)
9. Statystyki pliku, jak ma wiele edytorów, ale są statystyki charakterystyczne do artykułu korzystające z formatowania STC.
10. Na bieżąco aktualizowany pasek adresu (ang. breadcrumb) wskazujący miejsce w strukturze dokumentu w formatowaniu STC, na którym można kliknąć.
11. Śledzenie linijek ze zmianami (przez użycie zewnętrznej biblioteki [pydifflib-cpp](https://github.com/dominicprice/pydifflib-cpp)).

### Funkcjonalności edytora
1. Wczytywanie i zapisywanie plików
2. Zapamiętywanie ostatnio otwieranych plików
3. Wykrywanie zmian pliku z zewnątrz
4. Operowanie na UTF-8
5. Dodanie numeracji zaznaczonych linii (menu kontekstowe edytora)
6. Możliwość połączenia wielu linii w jedną oddzieloną spacją
7. Kopiowanie do schowka nazwy pliku zarówno jako basenam, jak i absolutnej.
8. TAB przesuwa cały zaznaczony tekst w prawo dodając spacje, SHIFT + TAB przesuwa w lewo (usuwając spacje)
9. Lista skrótów (wyświetlalna z menu aplikacji).
10. Wyszukiwanie wystąpień tekstu z możliwością wyboru czy z uwzględnieniem wilkości liter, oraz czy tylko całe słowa. Dodatkowo wyświetlane są informacje liczbowe:
    - liczba wystąpień szukanego tekstu **z** uwzględnieniem wielkości znaku
    - liczba wystąpień szukanego tekstu **bez** uwzględnienia wielkości znaków
    - liczba wystąpień szukanego tekstu **z** uwzględnieniem wielkości znaku **jako całe słowa**
    - liczba wystąpień szukanego tekstu **bez** uwzględnienia wielkości znaków **jako całe słowa**
11. Powiększanie i pomniejszanie czcionki przez `CTRL +` I `CTRL -`.
12. W pasku programu informacja o liczbie zmienionych, aczkolwiek niezapisanych linii, oraz o czasie wprowadzenia zmian w pliku, a także zapisu pliku (informacje dostępne tylko jeśli niezapisane zmiany).

## Współpraca
Chętnie zaakceptuje propozycje zmian do tego kodu aby był to wygodniejszy w użyciu edytor.

## Czy takie coś faktycznie się przydaje?
Mi się to bardzo przydało w ostatnich dwóch artykułach, m.in. [porównującym python i C++](https://cpp0x.pl/artykuly/Inne-artykuly/Porownanie-C++-i-Python-roznice-w-skladni-i-podejsciu-programistycznym/99) - tam co chwilę musiałem wrzucać znaczniki otaczające kod, czy tabelkę. Ja osobiście nie lubię wpisywać znaczników, potem szukać, którego z nich nie zamknąłem.

## Przydatne linki:
[Interpreter STC on-line](https://cpp0x.pl/stc/)
[Źródło użytych ikonek: MDI](https://pictogrammers.com/library/mdi/)


# Gwarancja
Program jest zrobiony tak po prostu, sam mu nie do końca ufam (że nie zgubi mi ważnych treści), dlatego nie udzielam gwarancji na jego użytkowanie.
Użycie na własne ryzyko ... co prawda mnie nie zawiódł jeszcze, no ale.
