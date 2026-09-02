import matplotlib.pyplot as plt
import re

# Read the data from your log file or paste it as a string
# For this example, I'll assume you paste the data into a string variable
data_string = """Total frame time: 3496 ┬╡s
Render time: 1916 ┬╡s
Physics time: 46 ┬╡s
Total frame time: 3246 ┬╡s
Render time: 526 ┬╡s
Physics time: 8 ┬╡s
Total frame time: 845 ┬╡s
Render time: 422 ┬╡s
Physics time: 76 ┬╡s
Total frame time: 1081 ┬╡s
Render time: 632 ┬╡s
Physics time: 48 ┬╡s
Total frame time: 2225 ┬╡s
Render time: 780 ┬╡s
Physics time: 67 ┬╡s
Total frame time: 2336 ┬╡s
Render time: 640 ┬╡s
Physics time: 15 ┬╡s
Total frame time: 1319 ┬╡s
Render time: 1231 ┬╡s
Physics time: 71 ┬╡s
Total frame time: 2975 ┬╡s
Render time: 1094 ┬╡s
Physics time: 68 ┬╡s
Total frame time: 2619 ┬╡s
Render time: 2134 ┬╡s
Physics time: 50 ┬╡s
Total frame time: 3690 ┬╡s
Render time: 390 ┬╡s
Physics time: 9 ┬╡s
Total frame time: 628 ┬╡s
Render time: 1183 ┬╡s
Physics time: 58 ┬╡s
Total frame time: 2618 ┬╡s
Render time: 932 ┬╡s
Physics time: 49 ┬╡s
Total frame time: 1865 ┬╡s
Render time: 770 ┬╡s
Physics time: 69 ┬╡s
Total frame time: 2435 ┬╡s
Render time: 780 ┬╡s
Physics time: 37 ┬╡s
Total frame time: 2204 ┬╡s
Render time: 1264 ┬╡s
Physics time: 341 ┬╡s
Total frame time: 3621 ┬╡s
Render time: 823 ┬╡s
Physics time: 13 ┬╡s
Total frame time: 1399 ┬╡s
Render time: 2313 ┬╡s
Physics time: 178 ┬╡s
Total frame time: 3842 ┬╡s
Render time: 1054 ┬╡s
Physics time: 48 ┬╡s
Total frame time: 1996 ┬╡s
Render time: 2031 ┬╡s
Physics time: 44 ┬╡s
Total frame time: 3128 ┬╡s
Render time: 842 ┬╡s
Physics time: 44 ┬╡s
Total frame time: 1782 ┬╡s
Render time: 691 ┬╡s
Physics time: 25 ┬╡s
Total frame time: 1349 ┬╡s
Render time: 433 ┬╡s
Physics time: 17 ┬╡s
Total frame time: 691 ┬╡s
Render time: 458 ┬╡s
Physics time: 79 ┬╡s
Total frame time: 1120 ┬╡s
Render time: 571 ┬╡s
Physics time: 27 ┬╡s
Total frame time: 1238 ┬╡s
Render time: 378 ┬╡s
Physics time: 8 ┬╡s
Total frame time: 637 ┬╡s
Render time: 406 ┬╡s
Physics time: 19 ┬╡s
Total frame time: 773 ┬╡s
Render time: 532 ┬╡s
Physics time: 9 ┬╡s
Total frame time: 853 ┬╡s
Render time: 946 ┬╡s
Physics time: 10 ┬╡s
Total frame time: 1177 ┬╡s
Render time: 372 ┬╡s
Physics time: 9 ┬╡s
Total frame time: 767 ┬╡s
Render time: 554 ┬╡s
Physics time: 22 ┬╡s
Total frame time: 1286 ┬╡s
Render time: 2182 ┬╡s
Physics time: 70 ┬╡s
Total frame time: 4019 ┬╡s
Render time: 753 ┬╡s
Physics time: 146 ┬╡s
Total frame time: 2941 ┬╡s
Render time: 1880 ┬╡s
Physics time: 68 ┬╡s
Total frame time: 3811 ┬╡s
Render time: 2246 ┬╡s
Physics time: 71 ┬╡s
Total frame time: 4691 ┬╡s
Render time: 1122 ┬╡s
Physics time: 44 ┬╡s
Total frame time: 2167 ┬╡s
Render time: 1173 ┬╡s
Physics time: 44 ┬╡s
Total frame time: 2198 ┬╡s
Render time: 1916 ┬╡s
Physics time: 68 ┬╡s
Total frame time: 3378 ┬╡s
Render time: 1033 ┬╡s
Physics time: 48 ┬╡s
Total frame time: 2319 ┬╡s
Render time: 375 ┬╡s
Physics time: 8 ┬╡s
Total frame time: 606 ┬╡s
Render time: 958 ┬╡s
Physics time: 48 ┬╡s
Total frame time: 2464 ┬╡s
Render time: 1687 ┬╡s
Physics time: 68 ┬╡s
Total frame time: 4256 ┬╡s
Render time: 1283 ┬╡s
Physics time: 29 ┬╡s
Total frame time: 2364 ┬╡s
Render time: 2801 ┬╡s
Physics time: 76 ┬╡s
Total frame time: 7557 ┬╡s
Render time: 1966 ┬╡s
Physics time: 40 ┬╡s
Total frame time: 3210 ┬╡s
Render time: 1295 ┬╡s
Physics time: 58 ┬╡s
Total frame time: 2538 ┬╡s
Render time: 1383 ┬╡s
Physics time: 57 ┬╡s
Total frame time: 2898 ┬╡s
Render time: 1133 ┬╡s
Physics time: 27 ┬╡s
Total frame time: 1988 ┬╡s
Render time: 1049 ┬╡s
Physics time: 29 ┬╡s
Total frame time: 1803 ┬╡s
Render time: 715 ┬╡s
Physics time: 17 ┬╡s
Total frame time: 1169 ┬╡s
Render time: 655 ┬╡s
Physics time: 17 ┬╡s
Total frame time: 913 ┬╡s
Render time: 550 ┬╡s
Physics time: 11 ┬╡s
Total frame time: 874 ┬╡s
Render time: 415 ┬╡s
Physics time: 8 ┬╡s
Total frame time: 687 ┬╡s
Render time: 495 ┬╡s
Physics time: 17 ┬╡s
Total frame time: 1097 ┬╡s
Render time: 472 ┬╡s
Physics time: 11 ┬╡s
Total frame time: 941 ┬╡s
Render time: 1000 ┬╡s
Physics time: 61 ┬╡s
Total frame time: 2500 ┬╡s
Render time: 443 ┬╡s
Physics time: 11 ┬╡s
Total frame time: 808 ┬╡s
Render time: 1129 ┬╡s
Physics time: 56 ┬╡s
Total frame time: 2351 ┬╡s
Render time: 1055 ┬╡s
Physics time: 68 ┬╡s
Total frame time: 2434 ┬╡s
Render time: 668 ┬╡s
Physics time: 74 ┬╡s
Total frame time: 2096 ┬╡s
Render time: 778 ┬╡s
Physics time: 59 ┬╡s
Total frame time: 1709 ┬╡s
Render time: 957 ┬╡s
Physics time: 23 ┬╡s
Total frame time: 1415 ┬╡s
Render time: 1831 ┬╡s
Physics time: 69 ┬╡s
Total frame time: 3550 ┬╡s
Render time: 730 ┬╡s
Physics time: 12 ┬╡s
Total frame time: 1278 ┬╡s
Render time: 1222 ┬╡s
Physics time: 67 ┬╡s
Total frame time: 2817 ┬╡s
Render time: 570 ┬╡s
Physics time: 14 ┬╡s
Total frame time: 887 ┬╡s
Render time: 698 ┬╡s
Physics time: 57 ┬╡s
Total frame time: 1945 ┬╡s
Render time: 2321 ┬╡s
Physics time: 67 ┬╡s
Total frame time: 3497 ┬╡s
Render time: 1660 ┬╡s
Physics time: 21 ┬╡s
Total frame time: 2607 ┬╡s
Render time: 1600 ┬╡s
Physics time: 69 ┬╡s
Total frame time: 3012 ┬╡s
Render time: 1232 ┬╡s
Physics time: 67 ┬╡s
Total frame time: 3646 ┬╡s
Render time: 1034 ┬╡s
Physics time: 14 ┬╡s
Total frame time: 1239 ┬╡s
Render time: 1199 ┬╡s
Physics time: 68 ┬╡s
Total frame time: 3222 ┬╡s
Render time: 935 ┬╡s
Physics time: 30 ┬╡s
Total frame time: 1669 ┬╡s
Render time: 904 ┬╡s
Physics time: 70 ┬╡s
Total frame time: 2407 ┬╡s
Render time: 1156 ┬╡s
Physics time: 40 ┬╡s
Total frame time: 2248 ┬╡s
Render time: 379 ┬╡s
Physics time: 9 ┬╡s
Total frame time: 566 ┬╡s
Render time: 447 ┬╡s
Physics time: 33 ┬╡s
Total frame time: 1154 ┬╡s
Render time: 923 ┬╡s
Physics time: 32 ┬╡s
Total frame time: 1883 ┬╡s
Render time: 397 ┬╡s
Physics time: 8 ┬╡s
Total frame time: 702 ┬╡s
Render time: 587 ┬╡s
Physics time: 39 ┬╡s
Total frame time: 1624 ┬╡s
Render time: 1149 ┬╡s
Physics time: 69 ┬╡s
Total frame time: 2799 ┬╡s
Render time: 1469 ┬╡s
Physics time: 113 ┬╡s
Total frame time: 3356 ┬╡s
Render time: 1511 ┬╡s
Physics time: 26 ┬╡s
Total frame time: 2320 ┬╡s
Render time: 1009 ┬╡s
Physics time: 118 ┬╡s
Total frame time: 2570 ┬╡s
Render time: 728 ┬╡s
Physics time: 39 ┬╡s
Total frame time: 1932 ┬╡s
Render time: 761 ┬╡s
Physics time: 196 ┬╡s
Total frame time: 2143 ┬╡s
Render time: 379 ┬╡s
Physics time: 9 ┬╡s
Total frame time: 611 ┬╡s
Render time: 820 ┬╡s
Physics time: 36 ┬╡s
Total frame time: 1991 ┬╡s
Render time: 510 ┬╡s
Physics time: 11 ┬╡s
Total frame time: 931 ┬╡s
Render time: 913 ┬╡s
Physics time: 59 ┬╡s
Total frame time: 2538 ┬╡s
Render time: 1236 ┬╡s
Physics time: 21 ┬╡s
Total frame time: 1642 ┬╡s
Render time: 2015 ┬╡s
Physics time: 69 ┬╡s
Total frame time: 3947 ┬╡s
Render time: 695 ┬╡s
Physics time: 69 ┬╡s
Total frame time: 1988 ┬╡s
Render time: 2592 ┬╡s
Physics time: 68 ┬╡s
Total frame time: 4229 ┬╡s
Render time: 2755 ┬╡s
Physics time: 85 ┬╡s
Total frame time: 4435 ┬╡s
Render time: 3019 ┬╡s
Physics time: 128 ┬╡s
Total frame time: 4590 ┬╡s
Render time: 2274 ┬╡s
Physics time: 67 ┬╡s
Total frame time: 3840 ┬╡s
Render time: 2818 ┬╡s
Physics time: 69 ┬╡s
Total frame time: 4318 ┬╡s
Render time: 2140 ┬╡s
Physics time: 56 ┬╡s
Total frame time: 3250 ┬╡s
Render time: 1580 ┬╡s
Physics time: 47 ┬╡s
Total frame time: 3125 ┬╡s
Render time: 1997 ┬╡s
Physics time: 112 ┬╡s
Total frame time: 4004 ┬╡s
Render time: 380 ┬╡s
Physics time: 19 ┬╡s
Total frame time: 1304 ┬╡s
Render time: 1334 ┬╡s
Physics time: 27 ┬╡s
Total frame time: 2450 ┬╡s
Render time: 1502 ┬╡s
Physics time: 81 ┬╡s
Total frame time: 3133 ┬╡s
Render time: 466 ┬╡s
Physics time: 29 ┬╡s
Total frame time: 1515 ┬╡s
Render time: 1421 ┬╡s
Physics time: 49 ┬╡s
Total frame time: 2847 ┬╡s
Render time: 414 ┬╡s
Physics time: 10 ┬╡s
Total frame time: 631 ┬╡s
Render time: 420 ┬╡s
Physics time: 10 ┬╡s
Total frame time: 624 ┬╡s
Render time: 605 ┬╡s
Physics time: 55 ┬╡s
Total frame time: 1671 ┬╡s
Render time: 1180 ┬╡s
Physics time: 68 ┬╡s
Total frame time: 2873 ┬╡s
Render time: 1655 ┬╡s
Physics time: 27 ┬╡s
Total frame time: 2890 ┬╡s
Render time: 1398 ┬╡s
Physics time: 32 ┬╡s
Total frame time: 2448 ┬╡s
Render time: 1167 ┬╡s
Physics time: 32 ┬╡s
Total frame time: 2440 ┬╡s
Render time: 2224 ┬╡s
Physics time: 69 ┬╡s
Total frame time: 4254 ┬╡s
Render time: 618 ┬╡s
Physics time: 17 ┬╡s
Total frame time: 1011 ┬╡s
Render time: 911 ┬╡s
Physics time: 22 ┬╡s
Total frame time: 1469 ┬╡s
Render time: 4494 ┬╡s
Physics time: 50 ┬╡s
Total frame time: 5617 ┬╡s
Render time: 1681 ┬╡s
Physics time: 70 ┬╡s
Total frame time: 2838 ┬╡s
Render time: 1458 ┬╡s
Physics time: 58 ┬╡s
Total frame time: 3066 ┬╡s
Render time: 954 ┬╡s
Physics time: 43 ┬╡s
Total frame time: 1881 ┬╡s
Render time: 2207 ┬╡s
Physics time: 38 ┬╡s
Total frame time: 3243 ┬╡s
Render time: 1136 ┬╡s
Physics time: 39 ┬╡s
Total frame time: 2087 ┬╡s
Render time: 1375 ┬╡s
Physics time: 65 ┬╡s
Total frame time: 2970 ┬╡s
Render time: 1532 ┬╡s
Physics time: 68 ┬╡s
Total frame time: 3169 ┬╡s
Render time: 1166 ┬╡s
Physics time: 45 ┬╡s
Total frame time: 2430 ┬╡s
Render time: 1912 ┬╡s
Physics time: 69 ┬╡s
Total frame time: 3618 ┬╡s
Render time: 1328 ┬╡s
Physics time: 8 ┬╡s
Total frame time: 1489 ┬╡s
Render time: 387 ┬╡s
Physics time: 9 ┬╡s
Total frame time: 610 ┬╡s
Render time: 753 ┬╡s
Physics time: 10 ┬╡s
Total frame time: 962 ┬╡s
Render time: 732 ┬╡s
Physics time: 14 ┬╡s
Total frame time: 1073 ┬╡s
Render time: 401 ┬╡s
Physics time: 19 ┬╡s
Total frame time: 632 ┬╡s
Render time: 494 ┬╡s
Physics time: 13 ┬╡s
Total frame time: 983 ┬╡s
Render time: 555 ┬╡s
Physics time: 51 ┬╡s
Total frame time: 1969 ┬╡s
Render time: 1153 ┬╡s
Physics time: 48 ┬╡s
Total frame time: 2608 ┬╡s
Render time: 1739 ┬╡s
Physics time: 49 ┬╡s
Total frame time: 3778 ┬╡s
Render time: 547 ┬╡s
Physics time: 37 ┬╡s
Total frame time: 1814 ┬╡s
Render time: 1621 ┬╡s
Physics time: 43 ┬╡s
Total frame time: 2658 ┬╡s
Render time: 1996 ┬╡s
Physics time: 36 ┬╡s
Total frame time: 3348 ┬╡s
Render time: 1276 ┬╡s
Physics time: 35 ┬╡s
Total frame time: 2453 ┬╡s
Render time: 2514 ┬╡s
Physics time: 67 ┬╡s
Total frame time: 4012 ┬╡s
Render time: 1751 ┬╡s
Physics time: 23 ┬╡s
Total frame time: 2233 ┬╡s
Render time: 1179 ┬╡s
Physics time: 57 ┬╡s
Total frame time: 2330 ┬╡s
Render time: 454 ┬╡s
Physics time: 35 ┬╡s
Total frame time: 1174 ┬╡s
Render time: 593 ┬╡s
Physics time: 32 ┬╡s
Total frame time: 1457 ┬╡s
Render time: 892 ┬╡s
Physics time: 57 ┬╡s
Total frame time: 2328 ┬╡s
Render time: 1031 ┬╡s
Physics time: 73 ┬╡s
Total frame time: 2406 ┬╡s
Render time: 2316 ┬╡s
Physics time: 87 ┬╡s
Total frame time: 3797 ┬╡s
Render time: 2214 ┬╡s
Physics time: 31 ┬╡s
Total frame time: 2900 ┬╡s
Render time: 362 ┬╡s
Physics time: 9 ┬╡s
Total frame time: 629 ┬╡s
Render time: 1924 ┬╡s
Physics time: 57 ┬╡s
Total frame time: 3426 ┬╡s
Render time: 1396 ┬╡s
Physics time: 67 ┬╡s
Total frame time: 3322 ┬╡s
Render time: 1598 ┬╡s
Physics time: 68 ┬╡s
Total frame time: 3225 ┬╡s
Render time: 3288 ┬╡s
Physics time: 30 ┬╡s
Total frame time: 4081 ┬╡s
Render time: 1365 ┬╡s
Physics time: 56 ┬╡s
Total frame time: 2718 ┬╡s
Render time: 1069 ┬╡s
Physics time: 33 ┬╡s
Total frame time: 2030 ┬╡s
Render time: 652 ┬╡s
Physics time: 19 ┬╡s
Total frame time: 1632 ┬╡s
Render time: 686 ┬╡s
Physics time: 15 ┬╡s
Total frame time: 1152 ┬╡s
Render time: 2042 ┬╡s
Physics time: 71 ┬╡s
Total frame time: 3835 ┬╡s
Render time: 1388 ┬╡s
Physics time: 67 ┬╡s
Total frame time: 2677 ┬╡s
Render time: 2168 ┬╡s
Physics time: 58 ┬╡s
Total frame time: 2988 ┬╡s
Render time: 553 ┬╡s
Physics time: 44 ┬╡s
Total frame time: 2051 ┬╡s
Render time: 412 ┬╡s
Physics time: 10 ┬╡s
Total frame time: 649 ┬╡s
Render time: 529 ┬╡s
Physics time: 40 ┬╡s
Total frame time: 1705 ┬╡s
Render time: 1168 ┬╡s
Physics time: 83 ┬╡s
Total frame time: 2917 ┬╡s
Render time: 514 ┬╡s
Physics time: 44 ┬╡s
Total frame time: 1787 ┬╡s
Render time: 805 ┬╡s
Physics time: 40 ┬╡s
Total frame time: 1890 ┬╡s
Render time: 419 ┬╡s
Physics time: 13 ┬╡s
Total frame time: 625 ┬╡s
Render time: 675 ┬╡s
Physics time: 47 ┬╡s
Total frame time: 1320 ┬╡s
Render time: 2757 ┬╡s
Physics time: 106 ┬╡s
Total frame time: 4645 ┬╡s
Render time: 2043 ┬╡s
Physics time: 68 ┬╡s
Total frame time: 3543 ┬╡s
Render time: 595 ┬╡s
Physics time: 39 ┬╡s
Total frame time: 1748 ┬╡s
Render time: 2459 ┬╡s
Physics time: 27 ┬╡s
Total frame time: 3136 ┬╡s
Render time: 1485 ┬╡s
Physics time: 64 ┬╡s
Total frame time: 2836 ┬╡s
Render time: 2192 ┬╡s
Physics time: 64 ┬╡s
Total frame time: 4037 ┬╡s
Render time: 2610 ┬╡s
Physics time: 79 ┬╡s
Total frame time: 4555 ┬╡s
Render time: 681 ┬╡s
Physics time: 66 ┬╡s
Total frame time: 2443 ┬╡s
Render time: 462 ┬╡s
Physics time: 10 ┬╡s
Total frame time: 659 ┬╡s
Render time: 1063 ┬╡s
Physics time: 30 ┬╡s
Total frame time: 2150 ┬╡s
Render time: 793 ┬╡s
Physics time: 45 ┬╡s
Total frame time: 1948 ┬╡s
Render time: 2347 ┬╡s
Physics time: 70 ┬╡s
Total frame time: 4224 ┬╡s
Render time: 746 ┬╡s
Physics time: 82 ┬╡s
Total frame time: 2286 ┬╡s
Render time: 866 ┬╡s
Physics time: 69 ┬╡s
Total frame time: 2400 ┬╡s
Render time: 597 ┬╡s
Physics time: 47 ┬╡s
Total frame time: 2134 ┬╡s
Render time: 831 ┬╡s
Physics time: 67 ┬╡s
Total frame time: 2496 ┬╡s
Render time: 1920 ┬╡s
Physics time: 68 ┬╡s
Total frame time: 3614 ┬╡s
Render time: 968 ┬╡s
Physics time: 49 ┬╡s
Total frame time: 2455 ┬╡s
Render time: 988 ┬╡s
Physics time: 44 ┬╡s
Total frame time: 1916 ┬╡s
Render time: 2810 ┬╡s
Physics time: 69 ┬╡s
Total frame time: 4532 ┬╡s
Render time: 1226 ┬╡s
Physics time: 70 ┬╡s
Total frame time: 2112 ┬╡s
Render time: 1155 ┬╡s
Physics time: 68 ┬╡s
Total frame time: 3157 ┬╡s
Render time: 1789 ┬╡s
Physics time: 66 ┬╡s
Total frame time: 2952 ┬╡s
Render time: 1047 ┬╡s
Physics time: 56 ┬╡s
Total frame time: 2577 ┬╡s
Render time: 857 ┬╡s
Physics time: 56 ┬╡s
Total frame time: 2060 ┬╡s
Render time: 570 ┬╡s
Physics time: 35 ┬╡s
Total frame time: 1698 ┬╡s
Render time: 852 ┬╡s
Physics time: 31 ┬╡s
Total frame time: 1495 ┬╡s
Render time: 2584 ┬╡s
Physics time: 59 ┬╡s
Total frame time: 3734 ┬╡s
Render time: 2167 ┬╡s
Physics time: 65 ┬╡s
Total frame time: 3281 ┬╡s
Render time: 2682 ┬╡s
Physics time: 72 ┬╡s
..."""  # Paste all your data here

# Parse the data
total_times = []
render_times = []
physics_times = []

# Split into lines and process each line
lines = data_string.strip().split('\n')
for i in range(0, len(lines), 3):
    if i+2 < len(lines):
        # Extract numbers using regex (handle the special character)
        total_match = re.search(r'Total frame time:\s*(\d+)', lines[i])
        render_match = re.search(r'Render time:\s*(\d+)', lines[i+1])
        physics_match = re.search(r'Physics time:\s*(\d+)', lines[i+2])
        
        if total_match and render_match and physics_match:
            total_times.append(int(total_match.group(1)))
            render_times.append(int(render_match.group(1)))
            physics_times.append(int(physics_match.group(1)))

# Create frame numbers
frames = list(range(1, len(total_times) + 1))

# Plot only first 200 frames
max_frames = min(200, len(frames))
frames_subset = frames[:max_frames]
total_subset = total_times[:max_frames]
render_subset = render_times[:max_frames]
physics_subset = physics_times[:max_frames]

# Create the plot
plt.figure(figsize=(12, 6))
plt.plot(frames_subset, total_subset, label='Total', linewidth=2, color='blue')
plt.plot(frames_subset, render_subset, label='Render', linewidth=1.5, color='green')
plt.plot(frames_subset, physics_subset, label='Physics', linewidth=1.5, color='red')

plt.xlabel('Frame Number')
plt.ylabel('Time (µs)')
plt.title('Frame Timing Breakdown (First 200 Frames)')
plt.legend()
plt.grid(True, alpha=0.3)

# Add a horizontal line for 16.67ms (60 FPS) if needed
# plt.axhline(y=16670, color='gray', linestyle='--', alpha=0.5, label='16.67ms (60 FPS)')

plt.tight_layout()
plt.show()

# Print some statistics
print(f"Total frames parsed: {len(total_times)}")
print(f"Showing first {max_frames} frames")
print(f"\nStatistics (first {max_frames} frames):")
print(f"  Total   - Mean: {sum(total_subset)/len(total_subset):.1f} µs, Min: {min(total_subset)} µs, Max: {max(total_subset)} µs")
print(f"  Render  - Mean: {sum(render_subset)/len(render_subset):.1f} µs, Min: {min(render_subset)} µs, Max: {max(render_subset)} µs")
print(f"  Physics - Mean: {sum(physics_subset)/len(physics_subset):.1f} µs, Min: {min(physics_subset)} µs, Max: {max(physics_subset)} µs")