//
//  GameViewController.swift
//  Quake-iOS
//
//  Created by Tom Kidd on 6/16/18.
//
//

import GLKit
import GameController

#if os(iOS)
import CoreMotion
#endif

public var gameInterrupted : Bool = false

public var startGame : Bool = false

class GameViewController: GLKViewController, GLKViewControllerDelegate
{
    var context: EAGLContext!

    var selectedGame = QuakeGame.Quake
    
    @IBOutlet weak var devModePanel: UIStackView!
    
    // TODO: standardize user defaults pattern
    let defaults = UserDefaults()
    
    @IBOutlet weak var startGameButton: UIButton!
    @IBOutlet weak var pauseButton: UIButton!
    
    // todo: do expansion packs better
    var additionalCommandLine:String = ""
    
    var gamePaused = false
    
    var quickLoadGame = false
    
    var demoMode = false
    
    var joysticksInitialized = false
    
    #if os(iOS)
    let motionManager: CMMotionManager = CMMotionManager()
    #endif
    
    override func viewDidLoad()
    {
        super.viewDidLoad()
        
        UserDefaults.standard.register(defaults: ["lanConfig_joinname" : "", "lanConfig_port" : 26000, "net_ipaddress" : "", "cl_name" : "", "sys_commandlineindex" : 1, "sys_commandline2" : "-game hipnotic -hipnotic", "sys_commandline3" : "-rogue", "sys_logmaxlines" : 1000, "sys_displayHelp" : false])
        
        self.context = EAGLContext(api: EAGLRenderingAPI.openGLES3)!
        EAGLContext.setCurrent(self.context)
        
        if self.context == nil {
            print("Failed to create ES context")
        }
        
        let view = self.view as! GLKView
        view.context = self.context
        view.delegate = self
        self.delegate = self
        view.enableSetNeedsDisplay = true
        view.drawableDepthFormat = .format24
        view.drawableMultisample = .multisampleNone
        view.drawableColorFormat = .RGBA8888
        view.drawableStencilFormat = .format8
        view.bindDrawable()
        self.preferredFramesPerSecond = 60
        
        #if os(iOS)
        self.navigationController?.navigationItem.backBarButtonItem?.isEnabled = false
        self.navigationController?.interactivePopGestureRecognizer?.isEnabled = false
        #endif
        
        #if os(tvOS)
        // note: this would prevent it from being accepted on the App Store
        
        let menuPressRecognizer = UITapGestureRecognizer()
        menuPressRecognizer.addTarget(self, action: #selector(GameViewController.menuButtonAction))
        menuPressRecognizer.allowedPressTypes = [NSNumber(value: UIPress.PressType.menu.rawValue)]
        
        self.view.addGestureRecognizer(menuPressRecognizer)
        
        #endif
        
        startup()
        
        startGameButton?.isHidden = !demoMode
        pauseButton?.isHidden = demoMode
    }
    
    @objc func menuButtonAction() {
        
    }

    
    override func didReceiveMemoryWarning() {
        super.didReceiveMemoryWarning()
        // Dispose of any resources that can be recreated.
        if EAGLContext.current() == self.context {
            EAGLContext.setCurrent(nil)
        }
    }
    
    override func viewDidAppear(_ animated: Bool)
    {
        super.viewDidAppear(animated)
        
        currentViewController = self
        
#if os(iOS)
        
        if !joysticksInitialized {
            
            let rect = view.frame
            let joystickSize = CGSize(width: 100.0, height: 100.0)
            let joystick1Frame = CGRect(origin: CGPoint(x: 50.0,
                                                        y: (rect.height - joystickSize.height - 50.0)),
                                        size: joystickSize)
            let joystick2Frame = CGRect(origin: CGPoint(x: rect.width - joystickSize.width - 50.0,
                                                        y: (rect.height - joystickSize.height - 50.0)),
                                        size: joystickSize)
            let joystick1 = JoyStickView(frame: joystick1Frame)
            joystick1.delegate = self
            
            view.addSubview(joystick1)
            
            joystick1.movable = false
            joystick1.alpha = 0.5
            joystick1.baseAlpha = 0.5 // let the background bleed thru the base
            joystick1.handleTintColor = UIColor.darkGray // Colorize the handle
            joystick1.joystickTag = .movePitch
            
            let joystick2 = JoyStickView(frame: joystick2Frame)
            joystick2.delegate = self
            
            view.addSubview(joystick2)
            
            joystick2.movable = false
            joystick2.alpha = 0.5
            joystick2.baseAlpha = 0.5 // let the background bleed thru the base
            joystick2.handleTintColor = UIColor.darkGray // Colorize the handle
            joystick2.joystickTag = .viewPitch
            
            let fireButton = UIButton(frame: CGRect(x: rect.width - 155 - joystickSize.width, y: rect.height - 90 - joystickSize.height, width: 75, height: 75))
            fireButton.setTitle("FIRE", for: .normal)
            fireButton.setBackgroundImage(UIImage(named: "JoyStickBase")!, for: .normal)
            fireButton.addTarget(self, action: #selector(firePressed), for: .touchDown)
            fireButton.addTarget(self, action: #selector(fireReleased), for: .touchUpInside)
            fireButton.alpha = 0.5
            
            view.addSubview(fireButton)
            
            let jumpButton = UIButton(frame: CGRect(x: rect.width - 90 - joystickSize.width, y: rect.height - 135 - joystickSize.height, width: 75, height: 75))
            jumpButton.setTitle("JUMP", for: .normal)
            jumpButton.setBackgroundImage(UIImage(named: "JoyStickBase")!, for: .normal)
            jumpButton.addTarget(self, action: #selector(jumpPressed), for: .touchDown)
            jumpButton.addTarget(self, action: #selector(jumpReleased), for: .touchUpInside)
            jumpButton.alpha = 0.5
            
            view.addSubview(jumpButton)
            
            joysticksInitialized = true
        }

#endif
        
        gamePaused = false
        
        if quickLoadGame {
            Sys_Key_Event(143, qboolean(1))
            Sys_Key_Event(143, qboolean(0))
            quickLoadGame = false
        }
        
        
        #if os(iOS)
        tiltaim_enabled = qboolean.init(UInt32(defaults.integer(forKey: "tiltAiming")))
        if defaults.integer(forKey: "tiltAiming") == 1 {
            motionManager.startDeviceMotionUpdates()
        }
        
        devModePanel.isHidden = !defaults.bool(forKey: "devMode")
        #endif

    }
    
    //MARK: GLKViewDelegate
    
    override func glkView(_ view: GLKView, drawIn rect: CGRect) {
        
        
    }
    
    // MARK: GLKViewControllerDelegate
    
    func glkViewControllerUpdate(_ controller: GLKViewController) {
        
        if !gamePaused {
            
            if sys_ended.rawValue != 0
            {
                setupEndingScreen()
                
                return
            }
            
            gl_translation = (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0)
            gl_projection = (0.839099705, 0.0, 0.0, 0.0, 0.0, 1.49173284, 0.0, 0.0, 0.0, 0.0, -1.00195503, -1.0, 0.0, 0.0, -8.00782013, 0.0)
            
            frame_lapse = Float(controller.timeSinceLastUpdate)
            
            #if os(iOS)
            if defaults.integer(forKey: "tiltAiming") == 1 {
                if let data = motionManager.deviceMotion {
                    in_pitchangle = Float(-data.attitude.roll)
                }
            }
            #endif
            
            Sys_Frame()
        }
    }
    
    
    override func viewWillDisappear(_ animated: Bool)
    {
        super.viewWillDisappear(animated)
        
        currentViewController = nil
    }
    
    func startup() {
        gl_screenwidth = Int32(UIScreen.main.bounds.size.width * UIScreen.main.scale)
        gl_screenheight = Int32(UIScreen.main.bounds.size.height * UIScreen.main.scale)
        
        let ipAddress = UserDefaults.standard.string(forKey: "net_ipaddress")
        
        if ipAddress != nil && !ipAddress!.isEmpty
        {
            net_ipaddress = UnsafeMutablePointer<Int8>.allocate(capacity: ipAddress!.count + 1)
            
            strcpy(net_ipaddress, ipAddress!.cString(using: String.defaultCStringEncoding)!)
        }
        
        var commandLineIndex = UserDefaults.standard.integer(forKey: "sys_commandlineindex")
        
        if commandLineIndex < 1 || commandLineIndex > 16
        {
            commandLineIndex = 1
        }
        
        var commandLine = UserDefaults.standard.string(forKey: "sys_commandline\(commandLineIndex)")
        
        if commandLine == nil
        {
            commandLine = ""
        }
        
        commandLine! += additionalCommandLine
        
        let resourcesDir = Bundle.main.resourcePath!
        
        GameDataDetection.detect()
        
        let documentsDir = try! FileManager().url(for: .documentDirectory, in: .userDomainMask, appropriateFor: nil, create: true).path
        
        var selectedGameString = "id1"
        
        switch selectedGame {
        case .Quake:
            selectedGameString = "id1"
        case .QuakeXP1:
            selectedGameString = "hipnotic"
        case .QuakeXP2:
            selectedGameString = "rogue"
        case .QuakeEp5:
            selectedGameString = "dopa"
        }
        
        host_initialized = qboolean(0)
        
        Sys_Init(resourcesDir, documentsDir, commandLine!, selectedGameString)
        
        if (host_initialized != qboolean(0)) {
            let server = UserDefaults.standard.string(forKey: "lanConfig_joinname")
            
            if server != nil && !server!.isEmpty
            {
                strcpy(lanConfig_joinname, server!.cString(using: String.defaultCStringEncoding)!)
            }
            
            let port = UserDefaults.standard.integer(forKey: "lanConfig_port")
            
            if port != 0
            {
                lanConfig_port = Int32(port)
            }
            
            let playerName = UserDefaults.standard.string(forKey: "cl_name")
            
            if playerName != nil && !playerName!.isEmpty
            {
                Sys_Cbuf_AddText("name \(playerName!)")
            }
            
            let logMaxLines = UserDefaults.standard.integer(forKey: "sys_logmaxlines")
            
            if logMaxLines != 0
            {
                sys_logmaxlines = Int32(logMaxLines)
            }
        }
    }
    
    func setupEndingScreen() {
    }
    
    @objc func firePressed(sender: UIButton!) {
        Sys_Key_Event(133, qboolean(1))
    }
    
    @objc func fireReleased(sender: UIButton!) {
        Sys_Key_Event(133, qboolean(0))
    }
    
    @objc func jumpPressed(sender: UIButton!) {
        Sys_Key_Event(13, qboolean(1))
    }
    
    @objc func jumpReleased(sender: UIButton!) {
        Sys_Key_Event(13, qboolean(0))
    }
    
    #if os(iOS)
    override var prefersHomeIndicatorAutoHidden: Bool {
        return true
    }
    
    override var preferredScreenEdgesDeferringSystemGestures: UIRectEdge {
        return .bottom
    }
    #endif
    
    @IBAction func exitToGameViewController(segue: UIStoryboardSegue) {
    }
    
    @IBAction func startGame(_ sender: UIButton) {
        demoMode = false
        startGameButton.isHidden = !demoMode
        pauseButton.isHidden = demoMode

        Sys_Cbuf_AddText("map start\n");
    }
    
    override func prepare(for segue: UIStoryboardSegue, sender: Any?) {
        if segue.identifier == "GamePausedSegue" {
            gamePaused = true
        }
    }

    @IBAction func dev_tilde(_ sender: UIButton) {
        Sys_Key_Event(96, qboolean(1)) // ~, true
        Sys_Key_Event(96, qboolean(0)) // ~, false
        becomeFirstResponder()
    }
    
    @IBAction func dev_up(_ sender: UIButton) {
        Sys_Key_Event(128, qboolean(1)) // K_UPARROW, true
        Sys_Key_Event(128, qboolean(0)) // K_UPARROW, false
    }
    
    @IBAction func dev_down(_ sender: UIButton) {
        Sys_Key_Event(129, qboolean(1)) // K_DOWNARROW, true
        Sys_Key_Event(129, qboolean(0)) // K_DOWNARROW, false
    }
    
    @IBAction func dev_left(_ sender: UIButton) {
        Sys_Key_Event(130, qboolean(1)) // K_LEFTARROW, true
        Sys_Key_Event(130, qboolean(0)) // K_LEFTARROW, false
    }
    
    @IBAction func dev_right(_ sender: UIButton) {
        Sys_Key_Event(131, qboolean(1)) // K_RIGHTARROW, true
        Sys_Key_Event(131, qboolean(0)) // K_RIGHTARROW, false
    }
    
    @IBAction func dev_esc(_ sender: UIButton) {
        Sys_Key_Event(27, qboolean(1)) // K_ESCAPE, true
        Sys_Key_Event(27, qboolean(0)) // K_ESCAPE, false
    }
    
    @IBAction func dev_enter(_ sender: UIButton) {
        Sys_Key_Event(13, qboolean(1)) // K_ENTER, true
        Sys_Key_Event(13, qboolean(0)) // K_ENTER, false
    }
    
    @IBAction func weaponUp(_ sender: UIButton) {
        Sys_Cbuf_AddText("impulse 10\n")
    }
    
    @IBAction func weaponDown(_ sender: UIButton) {
        Sys_Cbuf_AddText("impulse 12\n")
    }
    

    
}

extension Float {
    
    func compensate(_ value: Float) -> Float {
        return self * value
    }
    
}

#if os(iOS)

extension GameViewController: JoystickDelegate {
    
    func handleJoyStickPosition(tag: JoystickTag, x: CGFloat, y: CGFloat) {
        let compensation = Float(0.6)
        switch tag {
        case .viewPitch:
            in_rollangle = Float(x).compensate(compensation)
            in_pitchangle = Float(-y).compensate(compensation)
        case .movePitch:
            in_sidestepmove = Float(y)
            in_forwardmove = Float(x)
        }
         // misnamed but whatever
    }
    
    func handleJoyStick(tag: JoystickTag, angle: CGFloat, displacement: CGFloat) {
//        print("angle: \(angle) displacement: \(displacement)")
    }
    
}

extension GameViewController: UIKeyInput {

    override var canBecomeFirstResponder: Bool {
        return false // disable for now
    }
    
    var hasText: Bool {
        return false //???
    }
    
    func insertText(_ text: String) {
        Sys_Cbuf_AddText(text)
    }
    
    func deleteBackward() {
        Sys_Key_Event(127, qboolean(1)) // K_ESCAPE, true
        Sys_Key_Event(127, qboolean(0)) // K_ESCAPE, false
    }
    
    
}

#endif
