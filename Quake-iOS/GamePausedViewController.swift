//
//  GamePausedViewController.swift
//  Quake
//
//  Created by Tom Kidd on 7/7/18.
//

import UIKit

class GamePausedViewController: UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()

        // Do any additional setup after loading the view.
    }

    override func didReceiveMemoryWarning() {
        super.didReceiveMemoryWarning()
        // Dispose of any resources that can be recreated.
    }
    
    @IBAction func saveGame(_ sender: UIButton) {
        Sys_Key_Event(140, qboolean(1))
        Sys_Key_Event(140, qboolean(0))
        navigationController?.popViewController(animated: true)
    }
    
    @IBAction func loadGame(_ sender: UIButton) {
        Sys_Key_Event(143, qboolean(1))
        Sys_Key_Event(143, qboolean(0))
        navigationController?.popViewController(animated: true)
    }
    
    @IBAction func exitToMainMenu(_ sender: UIButton) {
//        CL_Disconnect ();
//        Host_ShutdownServer(qboolean(0));
        Host_Shutdown()
        navigationController?.popToRootViewController(animated: true)
    }
    

    /*
    // MARK: - Navigation

    // In a storyboard-based application, you will often want to do a little preparation before navigation
    override func prepare(for segue: UIStoryboardSegue, sender: Any?) {
        // Get the new view controller using segue.destinationViewController.
        // Pass the selected object to the new view controller.
    }
    */

}
