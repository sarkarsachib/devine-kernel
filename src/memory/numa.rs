use super::{PhysAddr, MemoryRegion};
use spin::Mutex;

#[cfg(not(test))]
use alloc::vec::Vec;

#[cfg(test)]
extern crate std;
#[cfg(test)]
use std::vec::Vec;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct NumaNode {
    pub id: usize,
}

#[derive(Debug, Clone)]
pub struct NumaMemoryRegion {
    pub region: MemoryRegion,
    pub node: NumaNode,
    pub distance: u8,
}

pub struct NumaTopology {
    nodes: Vec<NumaNode>,
    regions: Vec<NumaMemoryRegion>,
    distance_matrix: Vec<Vec<u8>>,
}

impl NumaTopology {
    pub const fn new() -> Self {
        NumaTopology {
            nodes: Vec::new(),
            regions: Vec::new(),
            distance_matrix: Vec::new(),
        }
    }

    pub fn add_node(&mut self, node: NumaNode) {
        self.nodes.push(node);
    }

    pub fn add_region(&mut self, region: NumaMemoryRegion) {
        self.regions.push(region);
    }

    pub fn set_distance(&mut self, from: NumaNode, to: NumaNode, distance: u8) {
        while self.distance_matrix.len() <= from.id {
            self.distance_matrix.push(Vec::new());
        }
        
        while self.distance_matrix[from.id].len() <= to.id {
            self.distance_matrix[from.id].push(255);
        }
        
        self.distance_matrix[from.id][to.id] = distance;
    }

    pub fn get_distance(&self, from: NumaNode, to: NumaNode) -> Option<u8> {
        self.distance_matrix
            .get(from.id)?
            .get(to.id)
            .copied()
    }

    pub fn get_node_regions(&self, node: NumaNode) -> Vec<&NumaMemoryRegion> {
        self.regions.iter()
            .filter(|r| r.node == node)
            .collect()
    }

    pub fn get_closest_node(&self, from: NumaNode) -> Option<NumaNode> {
        let mut closest = None;
        let mut min_distance = u8::MAX;

        for node in &self.nodes {
            if *node == from {
                continue;
            }
            
            if let Some(distance) = self.get_distance(from, *node) {
                if distance < min_distance {
                    min_distance = distance;
                    closest = Some(*node);
                }
            }
        }

        closest
    }

    pub fn allocate_from_node(&self, node: NumaNode, size: usize) -> Option<PhysAddr> {
        for region in self.get_node_regions(node) {
            if region.region.size >= size {
                return Some(region.region.start);
            }
        }
        None
    }

    pub fn get_node_for_address(&self, addr: PhysAddr) -> Option<NumaNode> {
        for region in &self.regions {
            if addr >= region.region.start && addr < region.region.end() {
                return Some(region.node);
            }
        }
        None
    }
}

pub static NUMA_TOPOLOGY: Mutex<NumaTopology> = Mutex::new(NumaTopology::new());

pub fn init_numa_topology() {
    let mut topology = NUMA_TOPOLOGY.lock();
    
    let node0 = NumaNode { id: 0 };
    topology.add_node(node0);
    
    topology.set_distance(node0, node0, 10);
}

pub fn get_current_numa_node() -> NumaNode {
    NumaNode { id: 0 }
}

pub fn allocate_numa_local(size: usize) -> Option<PhysAddr> {
    let current_node = get_current_numa_node();
    let topology = NUMA_TOPOLOGY.lock();
    topology.allocate_from_node(current_node, size)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_numa_topology() {
        let mut topology = NumaTopology::new();
        let node0 = NumaNode { id: 0 };
        let node1 = NumaNode { id: 1 };

        topology.add_node(node0);
        topology.add_node(node1);

        topology.set_distance(node0, node0, 10);
        topology.set_distance(node0, node1, 20);
        topology.set_distance(node1, node0, 20);
        topology.set_distance(node1, node1, 10);

        assert_eq!(topology.get_distance(node0, node1), Some(20));
        assert_eq!(topology.get_distance(node1, node0), Some(20));
        assert_eq!(topology.get_distance(node0, node0), Some(10));
    }
}
