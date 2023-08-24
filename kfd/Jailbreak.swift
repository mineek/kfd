//
//  Jailbreak.swift
//  kfd
//
//  Created by Mineek on 24/08/2023.
//

import Foundation

extension String: LocalizedError {
    public var errorDescription: String? { return self }
}

class Jailbreak {
    static let shared = Jailbreak()
    
    func jb() throws {
        let kfd = kopen_intermediate(2048, 1, 2, 2)
        guard kfd != 0 else {
            throw "Exploit failed!"
        }
        stage2(kfd)
    }
}
