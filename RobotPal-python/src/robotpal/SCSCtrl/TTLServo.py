from .._core.server import SimulatorServer

# [수정 포인트] 모듈이 로드될 때 서버 인스턴스를 하나 잡습니다.
_server = SimulatorServer.instance()

# ---------------------------------------------------------
# JeTank 원본 함수 인터페이스 유지 (클래스 없음!)
# ---------------------------------------------------------

def servoAngleCtrl(servoNum, angleInput, directionDebug, speedInput):
    """
    서보 모터 각도 제어 함수
    원본: offsetGenOut 계산 후 syncCtrl 호출
    변경: SimulatorServer로 명령 전송
    """
    # JeTank 로직: 방향(direction) 반영
    final_angle = angleInput * directionDebug
    
    # 시뮬레이터로 전송 (ID, Angle, Speed)
    _server.update_servo_value(servoNum, final_angle, speedInput)

    # 원본 코드와의 호환성을 위해 가짜 오프셋 값 리턴 (512가 중간값)
    return 512

def servoStop(servoNum):
    """서보 정지"""
    # 시뮬레이터에서는 딱히 할 게 없으므로 패스
    pass

def xyInput(xInput, yInput):
    """IK 좌표 입력 (필요 시 구현)"""
    # print(f"[Virtual SCSCtrl] xyInput: {xInput}, {yInput}")
    pass

def xyInputSmooth(xInput, yInput, dt):
    """부드러운 이동 (필요 시 구현)"""
    pass

# 원본 코드에 있는 다른 유틸리티 함수들도 필요하다면 빈 껍데기로라도 만들어둬야 함
def limitCheck(posInput, circlePos, circleLen, outline):
    # 원본 로직이 복잡하므로, 시뮬레이터에서 안 쓴다면 생략 가능
    # 만약 노트북에서 이 함수를 쓴다면 원본 코드를 그대로 복사해와야 함
    pass