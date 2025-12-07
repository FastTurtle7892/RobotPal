import asyncio
import websockets
import cv2
import numpy as np
import struct

PORT_WEB = 9999
PORT_TCP = 9998

# WebSocket 전용 큐
queue_web_raw = asyncio.Queue(maxsize=5)  # 수신만 빠르게 저장
queue_web = asyncio.Queue(maxsize=1)      # 디코딩 후 최신 프레임 저장

# TCP 전용 큐
queue_tcp = asyncio.Queue(maxsize=1)


# ---------------- WebSocket ----------------
async def websocket_receiver(websocket):
    print(f"[WEB] 클라이언트 연결: {websocket.remote_address}")
    try:
        async for message in websocket:
            if isinstance(message, bytes):
                if queue_web_raw.full():
                    await queue_web_raw.get()  # 오래된 메시지 제거
                await queue_web_raw.put(message)
            else:
                print(f"[WEB] 텍스트 메시지 수신: {message}")
                await websocket.send(f"Server received: {message}")
    except websockets.exceptions.ConnectionClosed:
        print("[WEB] 클라이언트 연결 종료")
    except Exception as e:
        print(f"[WEB] 예외 발생: {e}")


async def websocket_processor():
    while True:
        message = await queue_web_raw.get()
        if len(message) < 4:
            continue

        packet_len = struct.unpack('<L', message[:4])[0]
        jpeg_data = message[4:]

        if packet_len != len(jpeg_data):
            print(f"[WEB] 패킷 길이 불일치: header={packet_len}, actual={len(jpeg_data)}")

        nparr = np.frombuffer(jpeg_data, np.uint8)
        frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
        if frame is None:
            print("[WEB] 이미지 디코딩 실패")
            continue

        frame = cv2.flip(frame, 0)

        if queue_web.full():
            await queue_web.get()  # 기존 프레임 제거
        await queue_web.put(frame)


# ---------------- TCP ----------------
async def handle_tcp_client(reader, writer):
    addr = writer.get_extra_info('peername')
    print(f"[TCP] 클라이언트 연결됨 ({addr})")
    buffer = bytearray()

    try:
        while True:
            data = await reader.read(4096)
            if not data:
                print("[TCP] 연결 종료: 데이터 없음")
                break
            buffer.extend(data)

            while len(buffer) >= 4:
                msg_size = struct.unpack('<L', buffer[:4])[0]
                if len(buffer) < 4 + msg_size:
                    break
                frame_data = buffer[4:4 + msg_size]
                buffer = buffer[4 + msg_size:]

                nparr = np.frombuffer(frame_data, np.uint8)
                frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
                if frame is None:
                    print("[TCP] 이미지 디코딩 실패")
                    continue

                frame = cv2.flip(frame, 0)

                if queue_tcp.full():
                    await queue_tcp.get()
                await queue_tcp.put(frame)

    except Exception as e:
        print(f"[TCP] 예외 발생: {e}")
    finally:
        print(f"[TCP] 연결 종료 ({addr})")
        writer.close()
        await writer.wait_closed()


# ---------------- Display Manager ----------------
async def display_manager():
    print(">> 디스플레이 매니저 시작")
    while True:
        if not queue_web.empty():
            frame_web = await queue_web.get()
            cv2.imshow("WebSocket Stream", frame_web)

        if not queue_tcp.empty():
            frame_tcp = await queue_tcp.get()
            cv2.imshow("TCP Stream", frame_tcp)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            print(">> 'q' 키 입력됨. 종료합니다.")
            cv2.destroyAllWindows()
            break

        await asyncio.sleep(0.01)


# ---------------- Main ----------------
async def main():
    print(f"=== 통합 스트리밍 서버 시작 ===")
    server_ws = await websockets.serve(websocket_receiver, "0.0.0.0", PORT_WEB)
    server_tcp = await asyncio.start_server(handle_tcp_client, '0.0.0.0', PORT_TCP)

    processor_task = asyncio.create_task(websocket_processor())
    display_task = asyncio.create_task(display_manager())

    async with server_tcp:
        await asyncio.gather(
            server_tcp.serve_forever(),
            server_ws.wait_closed(),
            processor_task,
            display_task
        )


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n서버 종료")
        cv2.destroyAllWindows()
