-------------------------------------------------------------------------------------
**Universiteto užduotis**
Ši programa sukurta kaip universiteto užduotis, susidedanti iš dviejų dalių.

*1 užduotis. Užduoties funkcionalumo įgyvendinimas*
- Suprogramuoti užduotį taip, kad programa veiktų ir atitiktį pagrindinius reikalavimus (atliktų pagrindines funkcijas, turėtų sąsają su klaviatūra/pelyte, programa turėtų pradžią ir pabaigą).
- Nėra būtina, kad užduotis būtį atvaizduota su išbaigtu  grafiniu interfeisu. Vartotojo sąsaja gali būti ir paremta konsole.
- Kodas turėtu būti objektinis, t.y. sukurtas objektinis modelis, paskirstytos atsakomybės.

*2 užduotis. Refaktoringas*
Programa sukurta 1 užduotyje, turi būti išrefaktorinta taip, kad atitiktų šiuos reikalavimus.

1. OO koncepcijos (1 balas)
Panaudotos visos 4 pagrindinės OOP koncepcijos praktikoje:
- Inheritance (paveldėjimas): bent viena klasė paveldi iš kitos klasės ir perima arba papildo elgesį.
- Encapsulation (inkapsuliacija): duomenys slepiami privačiuose laukuose, prieiga vykdoma per metodus/getterius/setterius.
- Polymorphism (polimorfizmas): realiai panaudota situacija, kai keli skirtingi objektai gali būti naudojami per bendrą tipą (pvz. sąraše laikomi skirtingų tipų priešai, bet visi kviečiami tuo pačiu metodu update()).
- Abstraction (abstrakcija): panaudotas interfeisas arba abstrakti klasė, kuri apibrėžia bendrą elgesį, o konkrečios klasės jį realizuoja (pvz. interfeisas Enemy su metodu attack(), kurį kitaip įgyvendina skirtingi priešų tipai).

2. Švarus kodas ir DRY principas (2 balai)
- Klasės <= 200 eilučių, metodai <= eilučių.
- Nėra pasikartojančio kodo (DRY principas).
- Naudojamos konstantos, o ne "magic numbers".
- Nedidelis kodo sudėtingumas:
Vengiama įdėtų ciklų su keliais if/else.
Jei logika sudėtinga - ji padalinta į mažesnius metodus/klases.

3. Design pattern'ai (1 balas)
- Panaudotas bent vienas creational patter (pvz. Factory Method, Abstract Factory, Builder, Prototype)
- Singleton pattern naudoti negalima.
- Panaudotas bent vienas behavioural pattern (pvz. Strategy, Observer, State, Command, Template Method, Mediator).

4. Unit testai (1 balas)
- Sukurta 5-10 unit testų.
- Testai tikrina svarbiausią žaidimo logiką:
Objektų būsenų pasikeitimus (pvz. gyvybės sumažėjimą po atakos).
Skaičiavimus (pvz. taškų ar balanso pokyčius).
Sąveikas tarp objektų (pvz. ar durys atrakintos turint raktą).
- Testai turi būti paleidžiami su atitinkamomis priemonėmis (pvz. JUnit - Java, NUnit - C ir pan.).
- Visi testai turi būti vykdomi be klaidų.

Kosminis Kurjeris
Žaidėjas skraidina krovinį tarp planetų. Kiekvienas skrydis kainuoja kurą, o kelionės metu galinutikti įvairūs pavojingi įvykiai. Reikia pasirinkti saugiausią ir efektyviausią maršrutą.
Reikalavima:
- Žemėlapis sudarytas iš planetų (mazgų) ir tarp jų esančių maršrutų (briaunų).
- Kiekvienas maršrutas turi kuro ir rizikos lygį.
- Žaidėjas renkasi, į kurią planetą skristi toliau.
- Atsitiktiniai įvykiai: piratų užpuolimas, kosminė audra, kuro nuotėkis, navigacijos klaida.
- Žaidėjas turi ribotą kuro kiekį; kuro nepakanka - pralaimėjiams.
- Krovinys gali būti prarastas dėl įvykio - pralaimėjimas.
- Laimėjimas, jei krovinys pristatomas į paskirties planetą.
- Papildiniai: Draudimas krovinio rizikai, laivo modifikacijos.

-------------------------------------------------------------------------------------
**Realizavimas**

*Kosminis Kurjeris*
Kosminis Kurjeris – tai tekstinis kosminis nuotykių žaidimas, sukurtas universiteto užduočiai.
Žaidimo tikslas – įveikti 5 misijas, pristatant krovinius tarp planetų, tvarkantis su rizikingais skrydžiais, atsitiktiniais įvykiais, degalų ekonomija ir priešais.

Žaidimas parašytas C++ kalba, naudojant objektinį programavimą, design patterns, ir papildytas unit testais.

User interface yra konsole (CLI).

*Gameplay santrauka*
Žaidėjas valdo kosminį laivą, turintį:
- Degalus
- Kreditus
- Krovinį (jei prarandamas → pralaimėjimas)

Pasaulis sudarytas iš planetų grafų su maršrutais:
- kiekvienas maršrutas turi kuro kainą ir rizikos lygį

Kiekviename „round'e“ yra tikslas – pasiekti konkrečią planetą

Šuolio metu gali įvykti įvykiai:
- Cerberus (kova arba bėgimas)
- Reaper (instant lose su šansu)
- Storm (kuras ar krovinys)
- Navigation Failure (kuro praradimas)

Yra upgrades, perkami su kreditais, kurie gerina:
- kovos šansus
- kuro nuostolius
- rizikų pasekmes
- šansą išgyventi reaper encounter

Kiekviename raunde yra bonusai, kad žaidėjas progresuotų

*Paleidimas*
Windows (Visual Studio)
- Atidaryti solution (.sln)
- Set PSP kaip Startup Project
- Build & Run

*Unit testai*
Naudojamas: Microsoft Native Test Framework

Paleidimas:
- Build solution
- Tools → Test → Test Explorer
- Run all tests

Testų apimtis:
- skaičiavimai (risk, fuel, rewards)
- objektų būsenos pasikeitimai (fuel, cargo)
- sąveika tarp objektų
- event sistemos logika
- util metodų veikimas

Rezultatas: visi testai praeina sėkmingai
