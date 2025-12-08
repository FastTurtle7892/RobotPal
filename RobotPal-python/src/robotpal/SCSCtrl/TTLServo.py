from .._core.server import SimulatorServer


class TTLServo:
    """
    JeTank의 SCSCtrl.TTLServo 클래스를 시뮬레이터용으로 대체한 것.
    실제 시리얼 통신 대신 SimulatorServer로 명령을 보냄.
    """

    def __init__(self, port='/dev/ttyUSB0', baudrate=1000000, *args, **kwargs):
        # 시뮬레이터에서는 포트나 보드레이트가 필요 없지만,
        # 기존 코드 호환성을 위해 인자는 받아줍니다.
        print(f"[Virtual SCSCtrl] TTLServo 초기화됨 (Port: {port})")
        self.server = SimulatorServer.instance()

    def servoAngleCtrl(self, servo_id, angle, direction, speed):
        """
        특정 서보를 지정한 각도로 움직임

        :param servo_id: 서보 ID (1, 2, ...)
        :param angle: 목표 각도
        :param direction: 방향 (1: 정방향, -1: 역방향) - JeTank 로직에 따라 처리
        :param speed: 이동 속도
        """
        # 방향에 따른 각도 보정 (JeTank 원본 로직 참고)
        # 보통 direction이 -1이면 각도를 반전시키거나 오프셋을 줌
        final_angle = angle * direction

        # 서버로 전송
        self.server.update_servo_value(servo_id, final_angle, speed)

    def xyInput(self, x, y):
        """
        좌표 입력 (JeTank의 로봇팔 IK 제어 등에 사용될 수 있음)
        필요하다면 구현, 여기서는 로그만 출력
        """
        print(f"[Virtual SCSCtrl] XY Input: {x}, {y}")
        # self.server.send_command({"type": "ik", "x": x, "y": y}) # 예시

    def servoStop(self, servo_id):
        """서보 정지 (토크 해제 등)"""
        # 시뮬레이터에서는 특별히 할 게 없거나, type: stop 명령을 보냄
        pass