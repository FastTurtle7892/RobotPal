import os
import sys
import shutil
import tempfile
import threading
import asyncio
import time
import base64
import requests
import aiohttp
from aiohttp import web

# =================================================================
# [1] 환경 감지 및 기본 설정
# =================================================================
try:
    from google.colab import output
    import IPython.display
    from IPython.display import HTML, display as ipy_display, JSON, IFrame
    IS_COLAB = True
    IS_IPYTHON = True
except ImportError:
    IS_COLAB = False
    try:
        import IPython.display
        from IPython.display import IFrame, display as ipy_display
        IS_IPYTHON = True
    except ImportError:
        IS_IPYTHON = False
        def ipy_display(*args, **kwargs): pass

# 원래의 display 함수 백업 (패치 전)
_original_display = IPython.display.display if IS_IPYTHON else print

# =================================================================
# [2] 스마트 디스플레이 패치 (JS Polling 방식)
#     - 사용자가 제공한 '오류 수정 버전' 적용
# =================================================================
def smart_display(*objs, **kwargs):
    target = objs[0] if objs else None
    is_image_widget = False
    
    if target:
        try:
            import ipywidgets
            if isinstance(target, ipywidgets.Image):
                is_image_widget = True
        except: pass

    # Colab 환경이고 이미지 위젯인 경우 -> JS Polling 모드 작동
    if is_image_widget and IS_COLAB:
        widget_id = id(target)
        callback_name = f"get_frame_{widget_id}"
        
        # (1) 파이썬 콜백: 현재 데이터 반환
        def get_frame_callback():
            img_data = target.value
            if img_data:
                b64_str = base64.b64encode(img_data).decode('utf-8')
                return JSON({'b64': b64_str})
            return JSON({'b64': ''})
            
        # (2) 콜백 등록
        output.register_callback(callback_name, get_frame_callback)
        
        # (3) JS 코드 주입
        js_code = f"""
        <div id="container_{widget_id}">
            <img id="stream_{widget_id}" style="width:{target.width}px; height:{target.height}px; background:#000;"/>
        </div>
        <script>
        (function() {{
            var img = document.getElementById('stream_{widget_id}');
            var is_running = true;
            
            function updateFrame() {{
                if (!is_running || !document.body.contains(img)) return;
                
                google.colab.kernel.invokeFunction('{callback_name}', [], {{}})
                .then(function(result) {{
                    if (result.data && result.data['application/json']) {{
                        var b64 = result.data['application/json'].b64;
                        if (b64) img.src = "data:image/jpeg;base64," + b64;
                    }}
                    setTimeout(updateFrame, 33);
                }})
                .catch(function(err) {{
                    setTimeout(updateFrame, 1000);
                }});
            }}
            updateFrame();
        }})();
        </script>
        """
        ipy_display(HTML(js_code))
    else:
        # 그 외의 경우(로컬이거나 다른 위젯) 원래 함수 사용
        _original_display(*objs, **kwargs)

def apply_patch():
    """시스템의 display 함수를 스마트 버전으로 교체합니다."""
    if IS_COLAB:
        IPython.display.display = smart_display
        print("🚀 [RobotPal] Smart Display Patch Applied (JS Polling Mode)")

# =================================================================
# [3] 브리지 서버 클래스 (스마트 연결 대기 포함)
# =================================================================
class RobotPalBridge:
    def __init__(self, base_url="https://junwoo-seo-1998.github.io/RobotPal/"):
        self.base_url = base_url if base_url.endswith('/') else base_url + '/'
        self.download_dir = os.path.join(tempfile.gettempdir(), "RobotPal_ClientMode")
        self.targets = ["index.html", "RobotPal.js", "RobotPal.wasm", "RobotPal.data", "coi-serviceworker.min.js"]
        self.ws_browser = None
        self.ws_ml = None

    def _setup_files(self):
        if os.path.exists(self.download_dir):
            try: shutil.rmtree(self.download_dir)
            except: pass
        os.makedirs(self.download_dir, exist_ok=True)

        for f in self.targets:
            try:
                r = requests.get(self.base_url + f)
                if r.status_code == 200:
                    content = r.content
                    if f == "index.html":
                        html_str = content.decode('utf-8')
                        patch_script = """
                        <script>
                        (function() {
                            var protocol = window.location.protocol === 'https:' ? 'wss://' : 'ws://';
                            var CORRECT_URL = protocol + window.location.host + "/ws";
                            var OriginalWebSocket = window.WebSocket;
                            window.WebSocket = function(url, protocols) {
                                console.log("🔀 Bridge Redirect: " + CORRECT_URL);
                                return new OriginalWebSocket(CORRECT_URL, protocols);
                            };
                        })();
                        </script>
                        """
                        if '<head>' in html_str:
                            html_str = html_str.replace('<head>', '<head>' + patch_script, 1)
                        content = html_str.encode('utf-8')
                    with open(os.path.join(self.download_dir, f), "wb") as file:
                        file.write(content)
            except: pass

    async def _maintain_ml_connection(self):
        ML_URL = "ws://127.0.0.1:9999"
        # print(f"⏳ [대기] 웹 시뮬레이터가 켜지면 ML 서버({ML_URL})에 연결합니다...")

        while True:
            # 웹앱이 없으면 연결하지 않고 대기 (데이터 손실 방지)
            if self.ws_browser is None or self.ws_browser.closed:
                await asyncio.sleep(0.5)
                continue

            try:
                # print(f"⚡ 웹앱 감지됨! ML 서버에 연결 시도...")
                async with aiohttp.ClientSession() as session:
                    async with session.ws_connect(ML_URL) as ws:
                        # print(f"✅ ML 서버 연결 성공!")
                        self.ws_ml = ws
                        async for msg in ws:
                            if self.ws_browser is None or self.ws_browser.closed:
                                # print("⚠️ 웹앱 연결 끊김. ML 연결 중단.")
                                break 
                            if msg.type == aiohttp.WSMsgType.TEXT:
                                await self.ws_browser.send_str(msg.data)
                            elif msg.type == aiohttp.WSMsgType.BINARY:
                                await self.ws_browser.send_bytes(msg.data)
            except: pass
            
            self.ws_ml = None
            await asyncio.sleep(1)

    async def _handler_browser(self, request):
        ws = web.WebSocketResponse()
        await ws.prepare(request)
        self.ws_browser = ws
        try:
            async for msg in ws:
                if self.ws_ml and not self.ws_ml.closed:
                    if msg.type == web.WSMsgType.TEXT:
                        await self.ws_ml.send_str(msg.data)
                    elif msg.type == web.WSMsgType.BINARY:
                        await self.ws_ml.send_bytes(msg.data)
        finally:
            self.ws_browser = None
        return ws

    async def _handle_index(self, request):
        return web.FileResponse(os.path.join(self.download_dir, "index.html"))

    async def _on_header(self, request, response):
        response.headers['Cross-Origin-Opener-Policy'] = 'same-origin'
        response.headers['Cross-Origin-Embedder-Policy'] = 'require-corp'
        response.headers['Cache-Control'] = 'no-store'

    def _run_server_thread(self):
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        loop.create_task(self._maintain_ml_connection())

        app = web.Application()
        app.on_response_prepare.append(self._on_header)
        app.add_routes([
            web.get('/ws', self._handler_browser),
            web.get('/', self._handle_index),
            web.static('/', self.download_dir)
        ])

        runner = web.AppRunner(app)
        loop.run_until_complete(runner.setup())
        site = web.TCPSite(runner, '0.0.0.0', 8000)
        loop.run_until_complete(site.start())
        loop.run_forever()

    def start(self):
        # 1. 패치 적용
        apply_patch()
        
        # 2. 파일 준비 및 서버 스레드 시작
        self._setup_files()
        t = threading.Thread(target=self._run_server_thread, daemon=True)
        t.start()
        
        print("\n🚀 [RobotPal Bridge Started]")
        
        # 3. 환경별 화면 띄우기
        if IS_COLAB:
            output.serve_kernel_port_as_iframe(8000, height=800)
        elif IS_IPYTHON:
            print("🔗 Local Link: http://localhost:8000")
            try: ipy_display(IFrame("http://localhost:8000", width='100%', height=800))
            except: pass
        else:
            print("🌐 Open this URL in your browser: http://localhost:8000")

# =================================================================
# [4] 사용자가 호출할 범용 함수
# =================================================================
def start_bridge():
    """RobotPal 시뮬레이터 브리지를 시작합니다."""
    bridge = RobotPalBridge()
    bridge.start()