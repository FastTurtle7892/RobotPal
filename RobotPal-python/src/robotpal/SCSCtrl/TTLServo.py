import time
import numpy as np
from .._core.server import SimulatorServer

# --- [설정] 하드웨어 규격 (Real Code와 동일하게 맞춤) ---
linkageLenA = 90
linkageLenB = 160

# 서보 설정
servoNumCtrl = [0, 1]
servoDirection = [1, -1] 

servoInputRange = 850   # 0~1023 사이의 유효 범위 추정값
servoAngleRange = 180   # 180도
servoInit = [None, 512, 512, 512, 512, 512] # 초기 중심값

# [New] 가상 상태 추적용 변수 (Real Code의 nowPos 역할)
# 시뮬레이터는 실제 위치를 읽어올 수 없으므로, 마지막으로 보낸 명령을 현재 위치로 가정합니다.
current_raw_pos = [None, 512, 512, 512, 512, 512] 

_server = SimulatorServer.instance()

# ---------------------------------------------------------
# [Sim] 가상 드라이버 (업그레이드됨)
# ---------------------------------------------------------
def syncCtrl(ID_List, Speed_List, Goal_List):
    """
    Real Code의 syncCtrl과 호환되도록 설계
    Goal_List: 0~1023 Raw Value
    Speed_List: Raw Speed Value
    """
    for i in range(len(ID_List)):
        servo_id = ID_List[i]
        raw_goal = Goal_List[i]
        raw_speed = Speed_List[i]

        # 1. 시뮬레이터 전송을 위한 Degree 변환
        degree = (raw_goal - 512) * (servoAngleRange / servoInputRange)

        # 2. ID 5번(카메라) 방향 보정
        if servo_id == 5:
            degree = -degree 

        # 3. 속도 스케일링 (시뮬레이터 반응 속도에 맞춰 조정)
        # 실제 로봇의 500 속도는 시뮬레이터에서 너무 빠를 수 있으므로 적절히 줄임
        sim_speed = float(raw_speed) * 0.2
        if sim_speed <= 0: sim_speed = 0.5 # 최소 속도 보장

        # 4. 서버 전송
        _server.update_servo_value(servo_id, degree, sim_speed)
        
        # [New] 현재 위치 상태 업데이트 (다음 계산을 위해 저장)
        current_raw_pos[servo_id] = raw_goal

def get_raw_from_angle(ServoNum, AngleInput, DirectionDebug):
    """Real Code의 returnOffset 함수 역할"""
    return servoInit[ServoNum] + int((servoInputRange/servoAngleRange)*AngleInput*DirectionDebug)

# ---------------------------------------------------------
# [Logic] 수학 공식 (변경 없음, Real Code와 동일)
# ---------------------------------------------------------
def limitCheck(posInput, circlePos, circleLen, outline):
    circleRx = posInput[0]-circlePos[0]
    circleRy = posInput[1]-circlePos[1]
    realPosSquare = circleRx*circleRx+circleRy*circleRy
    shortRadiusSquare = np.square(circleLen[1]-circleLen[0])
    longRadiusSquare = np.square(circleLen[1]+circleLen[0])

    if realPosSquare >= shortRadiusSquare and realPosSquare <= longRadiusSquare:
        return posInput[0], posInput[1]
    else:
        lineK = (posInput[1]-circlePos[1])/(posInput[0]-circlePos[0]) if (posInput[0]-circlePos[0]) != 0 else 0
        lineB = circlePos[1]-(lineK*circlePos[0])
        
        if realPosSquare < shortRadiusSquare:
            aX = 1 + lineK*lineK
            bX = 2*lineK*(lineB - circlePos[1]) - 2*circlePos[0]
            cX = circlePos[0]*circlePos[0] + (lineB - circlePos[1])*(lineB - circlePos[1]) - shortRadiusSquare
            resultX = bX*bX - 4*aX*cX
            if resultX < 0: resultX = 0 
            x1 = (-bX + np.sqrt(resultX))/(2*aX)
            x2 = (-bX - np.sqrt(resultX))/(2*aX)
            y1 = lineK*x1 + lineB
            y2 = lineK*x2 + lineB
            dist1 = (posInput[0]-x1)**2 + (posInput[1]-y1)**2
            dist2 = (posInput[0]-x2)**2 + (posInput[1]-y2)**2
            if dist1 < dist2: return x1, y1
            else: return x2, y2

        elif realPosSquare > longRadiusSquare:
            unit_vec = np.array([circleRx, circleRy])
            norm = np.linalg.norm(unit_vec)
            if norm == 0: return posInput[0], posInput[1]
            scaled = unit_vec / norm * (circleLen[1]+circleLen[0] - outline)
            return circlePos[0] + scaled[0], circlePos[1] + scaled[1]
            
    return posInput[0], posInput[1]

def planeLinkageReverse(linkageLen, linkageEnDe, servoNum, debugPos, goalPos):
    # 좌표 보정
    goalPos[0] = goalPos[0] + debugPos[0]
    goalPos[1] = goalPos[1] + debugPos[1]

    AngleEnD = np.arctan(linkageEnDe/linkageLen[1])*180/np.pi
    linkageLenREAL = np.sqrt(((linkageLen[1]*linkageLen[1])+(linkageEnDe*linkageEnDe)))

    # Limit Check
    goalPos[0], goalPos[1] = limitCheck(goalPos, debugPos, [linkageLen[0], linkageLenREAL], 0.00001)

    if goalPos[0] >= 0:
        sqrtGenOut = np.sqrt(goalPos[0]*goalPos[0]+goalPos[1]*goalPos[1])
        if sqrtGenOut == 0: return [0, 0]

        nGenOut = (linkageLen[0]*linkageLen[0]+goalPos[0]*goalPos[0]+goalPos[1]*goalPos[1]-linkageLenREAL*linkageLenREAL)/(2*linkageLen[0]*sqrtGenOut)
        nGenOut = max(-1, min(1, nGenOut))
        angleA = np.arccos(nGenOut)*180/np.pi

        AB = goalPos[1]/goalPos[0] if goalPos[0] != 0 else 99999
        angleB = np.arctan(AB)*180/np.pi
        
        angleGenA = angleB - angleA

        mGenOut = (linkageLen[0]*linkageLen[0]+linkageLenREAL*linkageLenREAL-goalPos[0]*goalPos[0]-goalPos[1]*goalPos[1])/(2*linkageLen[0]*linkageLenREAL)
        mGenOut = max(-1, min(1, mGenOut))
        angleGenB = np.arccos(mGenOut)*180/np.pi - 90

        return [angleGenA*servoDirection[servoNumCtrl[0]], (angleGenB+AngleEnD)*servoDirection[servoNumCtrl[1]]]
    
    return [0, 0]

# ---------------------------------------------------------
# [User Interface] xyInputSmooth 구현 (핵심 업그레이드)
# ---------------------------------------------------------
def xyInputSmooth(xInput, yInput, dt):
    """
    [Simulated Smooth Control]
    Real Code와 동일하게 시간(dt)을 입력받아 각 모터의 속도를 자동 계산합니다.
    """
    # 1. 역기구학 계산 (x, y -> 각도)
    # yInput 부호 반전 주의 (-yInput)
    angGenOut = planeLinkageReverse([linkageLenA, linkageLenB], 0, servoNumCtrl, [0,0], [xInput, -yInput])
    
    # 2. 목표 Raw 값 계산
    # JetTank는 ID 2번에 +90도 오프셋, ID 3번은 그대로 사용
    target_raw_2 = get_raw_from_angle(2, angGenOut[0] + 90, 1)
    target_raw_3 = get_raw_from_angle(3, angGenOut[1], -1)

    # 3. 속도 자동 계산 (Distance / Time)
    # 현재 위치(current_raw_pos)와 목표 위치의 차이를 구함
    diff_2 = abs(target_raw_2 - current_raw_pos[2])
    diff_3 = abs(target_raw_3 - current_raw_pos[3])

    # 0으로 나누기 방지 및 정수형 변환
    speed_2 = int(diff_2 / dt) if dt > 0 else 200
    speed_3 = int(diff_3 / dt) if dt > 0 else 200

    # 4. 명령 전송 (동기화)
    # 두 모터에 대해 계산된 속도와 목표 위치를 한 번에 전송
    syncCtrl([2, 3], [speed_2, speed_3], [target_raw_2, target_raw_3])

    return [angGenOut[0], angGenOut[1]]

# 호환성을 위한 기존 함수 유지 (기본 속도 사용)
def xyInput(xInput, yInput):
    return xyInputSmooth(xInput, yInput, 1.0) # 기본 1초 이동

def servoStop(n): pass
def portClose(): pass