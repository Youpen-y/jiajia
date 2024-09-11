### Next generation JIAJIA (SDSM) --- JIAJIA3.0
JIAJIA is a software distributed shared memory system that written at the beginning of the century. What makes it unique is its lock-based consistency.

As a static library, it provide some usable interfaces to programmer so that they can use it divide their task to run on multiple machines. [Interfaces](#jiajia30-interfaces)

What I want to do is mainly include these aspects:

- Upgrade it so that it can be adapted to 64-bit machines
- Redesign some interfaces to increase usability
- Optimize cache design to support LRU
- Adopt RDMA technology to redesign the whole system desing (two directions)
  - RDMA support as a extra function that can be truned on or off. It means that there are two side-by-side network protocol stacks in the system, and will use RDMA first if possible.
  - Pure RDMA (pursue extreme performance)
- The last one need to consider is its availability (fault tolerance), consider checkpoint(store it's memory content to persistent storage periodically)

### JIAJIA3.0 Interfaces



### Work up to Now
- Upgrade origin code with detailed comments
- Remove obsolete code (functions or usages) snippets
- Adapt it to 64bit machine (redesign msg )