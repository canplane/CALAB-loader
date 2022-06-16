# CALAB-loader

[CaSys Lab] 석사 신입생 선발 프로젝트 1차 - 프로그래밍 랩

* 진행 기간: 2021-11-04 ~ 2021-11-14
* Mentor: Bokyung Cha


## 1. 구현 완료 상황

* 요구 사항이었던 모든 파트를 성공적으로 구현하였습니다.
  - Part 1: All-at-once loading, Demand loading, Memory access errors, Interface
  - Part 2: Return to the program loader (Option 1, 2 모두), Back-to-back loading, User-level threading

* 모든 파트에 대해서 All-at-once / demand loader를 모두 구현하였습니다. (apager.c, dpager.c)
* 모든 파트에 대해서 makefile을 생성하였습니다. (컴파일: $ make [apager | dpager | test], 실행: $ make [run_apager | run_dpager])

* 최대한 예외 처리를 하려 노력하였고, loader와 그 위에서 동작하는 프로그램의 출력 결과를 명확히 구분하기 위해 폰트 스타일링을 하였습니다.

* 첨부드릴 소스 코드의 주요 디렉터리 구조는 다음과 같습니다.

      part-1/ (Part 1)
        apager.c
        dpager.c
        common.c (All-at-once, Demand loader에서 공통되는 부분을 분리한 코드)
        Makefile
        example/ (테스트 코드 디렉터리)
          my_test.c (직접 작성한 테스트 코드)

      part-2/ (Part 2)
        option-1/ (Option 1: return_to_loader() 방식)
          2-back_to_back/ (Back-to-back loader)
            apager.c
            dpager.c
            common.c (All-at-once, Demand loader에서 공통되는 부분을 분리한 코드)
            Makefile

          3-thread/ (User-level threading)
            apager.c
            dpager.c
            common.c (All-at-once, Demand loader에서 공통되는 부분을 분리한 코드)
            queue.c (스케줄링에서 쓰이는 Ready queue가 기반하는 generic Queue 자료구조 구현 코드)
            Makefile

          interrupt.c (return_to_loader(), yield()가 정의되어 있고 테스트 코드 단에 include되는 코드)

          example/ (테스트 코드 디렉터리)
            yield/ (User-level threading에서 yield() 테스트 코드)
              a.c (A-1 -> A-2 -> A-3 -> return)
              b.c (B-1 -> B-2 -> B-3 -> return)
              c.c (C-1 -> C-2 -> C-3 -> return)

            … (Part 2 테스트용으로 주어진 코드들)
            my_test.c (직접 작성한 테스트 코드)

        option-2/ (소스 코드 수정이 없는 방식)
          2-back_to_back/ (Back-to-back loader)
            apager.c
            dpager.c
            common.c (All-at-once, Demand loader에서 공통되는 부분을 분리한 코드)
            Makefile

          example/ (테스트 코드 디렉터리)
            … (Part 2 테스트용으로 주어진 코드들)
            my_test.c (직접 작성한 테스트 코드)


## 2. 테스트 통과 상황

* Part 1의 모든 구현 코드는 제가 직접 작성한 테스트 파일(my_test.c)에 대해서 정상 작동을 확인하였습니다.
  - 제가 직접 작성한 테스트 파일(my_test.c)에 대해서 정상 작동하였습니다.
  - Demand loader에서, 진짜로 segmentation fault가 발생하는 상황을 signal handling 함수에서 적절히 처리하여 종료시킴을 확인하였습니다.

* Part 2의 모든 구현 코드는 다음과 같은 테스트를 모두 거쳤습니다.
  - Part 2 테스트용으로 전달받은 코드들(segfault.c 제외)과, 직접 작성한 테스트 코드(my_test.c)를 같이 실행하였을 때 정상 작동하였습니다.
  - 제가 직접 작성한 테스트 파일(my_test.c)를 8번 중복 실행했을 때 정상 작동하였습니다.
  - User-level threading의 경우, 직접 작성한 yield/a.c, yield/b.c, yield/c.c 3개의 코드에서 yield()를 교대로 잘 호출하는 것을 확인하였습니다.


## 3. 진행 상황 

* Part 2에서 테스트 코드를 수정하여 loader로 복귀하는 방안은 system call과 ISR에 착안했습니다.
  구현한 ISR 함수의 주소를 환경 변수로 넘기고, setjmp(), longjmp()를 통해 현재 맥락을 저장하고 상대편으로 jump하도록 하는 방식으로 구현 성공했습니다.

* Part 2의 Back-to-back loader에서 테스트 코드의 수정 없이 loader로 다시 복귀하는 방안은 loader를 fork시키는 방식으로 구현 성공하였습니다.

* Part 2의 Return to the program loader 코드는 Back-to-back loading 코드의 부분 집합이라 생각하여,
  따로 분리하지 않고 Back-to-back loading부터 구현 성공하였습니다.

* Part 2의 User-level threading의 경우, 최대 8개의 thread까지 실행할 수 있도록 하였습니다.
  단, static linking 기반이므로 각 thread는 0x1000000 ~ 0x5000000의 공간을 균등하게 나눈 영역에 적절히 load되도록 컴파일 옵션을 적용시켰습니다.
