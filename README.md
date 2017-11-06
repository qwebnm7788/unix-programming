# unix-programming
shell project

* sleep 10 &
* sleep 10 &
* sleep 5
* 순서대로 입력 시 foreground 입력이 제대로 작동하지 않는 문제
-> 
waitpid에 -1을 첫번째 인자로 넘겨주게 되면 자식 프로세스 중 아무거나 하나 종료된 것을 가져오게 되고
마찬가지로 wait 함수 또한 특정자식이 아닌 종료된 임의의 자식 프로세스를 가져오게 된다. 작성한 코드는 항상 부모 프로세스가
동일한 형태로 구현되어 있기 때문에 이전 입력에서 background에서 작업중인 프로세스가 foreground 자식의 wait에 영향을 줄 수 있다.
