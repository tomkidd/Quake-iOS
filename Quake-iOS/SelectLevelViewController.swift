//
//  SelectLevelViewController.swift
//  Quake
//
//  Created by Tom Kidd on 7/2/18.
//

import UIKit

class SelectLevelViewController: UIViewController {
    
    var selectedGame:QuakeGame!
    var selectedEpisode:Int = 1
    var commandLine = ""

    @IBOutlet weak var boxArtImage: UIImageView!
    @IBOutlet weak var levelList: UITableView!

    let quakeLevels:[[(map: String, name: String)]] = [[(map: "e1m1", name: "E1M1: the Slipgate Complex"),
                                                        (map: "e1m2", name: "E1M2: Castle of the Damned"),
                                                        (map: "e1m3", name: "E1M3: the Necropolis"),
                                                        (map: "e1m4", name: "E1M4: the Grisly Grotto"),
                                                        (map: "e1m5", name: "E1M5: Gloom Keep"),
                                                        (map: "e1m6", name: "E1M6: The Door to Chthon"),
                                                        (map: "e1m7", name: "E1M7: The House of Chthon"),
                                                        (map: "e1m8", name: "E1M8: Ziggurat Vertigo")],
                                                       [(map: "e2m1", name: "E2M1: the Installation"),
                                                        (map: "e2m2", name: "E2M2: the Ogre Citadel"),
                                                        (map: "e2m3", name: "E2M3: the Crypt of Decay"),
                                                        (map: "e2m4", name: "E2M4: the Ebon Fortress"),
                                                        (map: "e2m5", name: "E2M5: the Wizard's Manse"),
                                                        (map: "e2m6", name: "E2M6: the Dismal Oubliette"),
                                                        (map: "e2m7", name: "E2M7: the Underearth")],
                                                       [(map: "e3m1", name: "E3M1: Termination Central"),
                                                        (map: "e3m2", name: "E3M2: The Vaults of Zin"),
                                                        (map: "e3m3", name: "E3M3: the Tomb of Terror"),
                                                        (map: "e3m4", name: "E3M4: Satan's Dark Delight"),
                                                        (map: "e3m5", name: "E3M5: the Wind Tunnels"),
                                                        (map: "e3m6", name: "E3M6: Chambers of Torment"),
                                                        (map: "e3m7", name: "E3M7: the Haunted Halls")],
                                                       [(map: "e4m1", name: "E4M1: the Sewage System"),
                                                        (map: "e4m2", name: "E4M2: The Tower of Despair"),
                                                        (map: "e4m3", name: "E4M3: The Elder God Shrine"),
                                                        (map: "e4m4", name: "E4M4: the Palace of Hate"),
                                                        (map: "e4m5", name: "E4M5: Hell's Atrium"),
                                                        (map: "e4m6", name: "E4M6: The Pain Maze"),
                                                        (map: "e4m7", name: "E4M7: Azure Agony"),
                                                        (map: "e4m8", name: "E4M8: the Nameless City")]]
    
    let quakeXP1Levels:[[(map: String, name: String)]] = [[(map: "hip1m1", name: "HIP1M1: The Pumping Station"),
                                                           (map: "hip1m2", name: "HIP1M2: Storage Facility"),
                                                           (map: "hip1m3", name: "HIP1M3: The Lost Mine"),
                                                           (map: "hip1m4", name: "HIP1M4: Research Facility"),
                                                           (map: "hip1m5", name: "HIP1M5: Military Complex")],
                                                          [(map: "hip2m1", name: "HIP2M1: Ancient Realms"),
                                                           (map: "hip2m2", name: "HIP2M2: The Black Cathedral"),
                                                           (map: "hip2m3", name: "HIP2M3: The Catacombs"),
                                                           (map: "hip2m4", name: "HIP2M4: The Crypt"),
                                                           (map: "hip2m5", name: "HIP2M5: Mortum's Keep"),
                                                           (map: "hip2m6", name: "HIP2M6: The Gremlin's Domain")],
                                                          [(map: "hip3m1", name: "HIP3M1: Tur Torment"),
                                                           (map: "hip3m2", name: "HIP3M2: Pandemonium"),
                                                           (map: "hip3m3", name: "HIP3M3: Limbo"),
                                                           (map: "hip3m4", name: "HIP3M4: The Gauntlet"),
                                                           (map: "hip3m5", name: "HIP3M5: Armagon's Lair"),
                                                           (map: "hip3m6", name: "HIP3M6: The Edge of Oblivion")]]
    
    let quakeXP2Levels:[[(map: String, name: String)]] = [[(map: "r1m1", name: "R1M1: Deviant's Domain"),
                                                           (map: "r1m2", name: "R1M2: Dread Portal"),
                                                           (map: "r1m3", name: "R1M3: Judgment Call"),
                                                           (map: "r1m4", name: "R1M4: Cave of Death"),
                                                           (map: "r1m5", name: "R1M5: Towers of Wrath"),
                                                           (map: "r1m6", name: "R1M6: Temple of Pain"),
                                                           (map: "r1m7", name: "R1M7: Tomb of the Overlord")],
                                                          [(map: "r2m1", name: "R2M1: Tempus Fugit"),
                                                           (map: "r2m2", name: "R2M2: Elemental Fury I"),
                                                           (map: "r2m3", name: "R2M3: Elemental Fury II"),
                                                           (map: "r2m4", name: "R2M4: Curse of Osiris"),
                                                           (map: "r2m5", name: "R2M5: Wizard's Keep"),
                                                           (map: "r2m6", name: "R2M6: Blood Sacrifice"),
                                                           (map: "r2m7", name: "R2M7: Last Bastion"),
                                                           (map: "r2m8", name: "R2M8: Source of Evil")]]
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        switch selectedGame {
        case .Quake:
            boxArtImage.image = UIImage(named: "quake-box")
        case .QuakeXP1:
            boxArtImage.image = UIImage(named: "quake-xp1-box")
        case .QuakeXP2:
            boxArtImage.image = UIImage(named: "quake-xp2-box")
        default: ()
        }
        
        levelList.register(UITableViewCell.self, forCellReuseIdentifier: "cell")
        
        levelList.backgroundColor = UIColor.black
        #if TARGET_OS_IPHONE
        episodeList.separatorStyle = .none
        #endif


        // Do any additional setup after loading the view.
    }

    override func didReceiveMemoryWarning() {
        super.didReceiveMemoryWarning()
        // Dispose of any resources that can be recreated.
    }
    
    @IBAction func startEpisode(_ sender: UIButton) {
        switch selectedGame {
        case .Quake:
            commandLine = "+map start"
        case .QuakeXP1:
            commandLine = "-game hipnotic -hipnotic +map start"
        case .QuakeXP2:
            commandLine = "-rogue +map start"
        default: ()
        }
        
        performSegue(withIdentifier: "StartLevelSegue", sender: self)
    }
    

    // MARK: - Navigation
    override func prepare(for segue: UIStoryboardSegue, sender: Any?) {
        if segue.identifier == "StartLevelSegue" {
            (segue.destination as! GameViewController).additionalCommandLine = commandLine
            (segue.destination as! GameViewController).selectedGame = selectedGame
        }
    }

}

extension SelectLevelViewController: UITableViewDelegate {
    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        switch selectedGame {
        case .Quake:
            commandLine = " +map \(quakeLevels[selectedEpisode][indexPath.row].map)"
        case .QuakeXP1:
            commandLine = "-game hipnotic -hipnotic +map \(quakeXP1Levels[selectedEpisode][indexPath.row].map)"
        case .QuakeXP2:
            commandLine = "-rogue +map \(quakeXP2Levels[selectedEpisode][indexPath.row].map)"
        default: ()
        }
        
        performSegue(withIdentifier: "StartLevelSegue", sender: self)
    }
}

extension SelectLevelViewController: UITableViewDataSource {
    
    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        switch selectedGame {
        case .Quake:
            return quakeLevels[selectedEpisode].count
        case .QuakeXP1:
            return quakeXP1Levels[selectedEpisode].count
        case .QuakeXP2:
            return quakeXP2Levels[selectedEpisode].count
        default: ()
        }
        
        return 0
    }
    
    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = levelList.dequeueReusableCell(withIdentifier: "cell", for: indexPath)

        cell.backgroundColor = UIColor.black
        cell.textLabel?.textColor = UIColor.white
        cell.textLabel?.font = UIFont(name: "DpQuake", size: (cell.textLabel?.font?.pointSize)!)

        switch selectedGame {
        case .Quake:
            cell.textLabel?.text = quakeLevels[selectedEpisode][indexPath.row].name
        case .QuakeXP1:
            cell.textLabel?.text = quakeXP1Levels[selectedEpisode][indexPath.row].name
        case .QuakeXP2:
            cell.textLabel?.text = quakeXP2Levels[selectedEpisode][indexPath.row].name
        default: ()
        }
        return cell

    }
}
