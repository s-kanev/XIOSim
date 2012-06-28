#include "BufferManager.h"


#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

ostream& operator<< (ostream &out, handshake_container_t &hand)
{
  out << "hand:" << " ";
  out << hand.valid;
  out << hand.mem_released;
  out << hand.isFirstInsn;
  out << hand.isLastInsn;
  out << hand.killThread;
  out << " ";
  out << hand.mem_buffer.size();
  out << " ";
  out << "pc:" << hand.handshake.pc << " "; 
  out << "coreid:" << hand.handshake.coreID << " "; 
  out << "npc:" << hand.handshake.npc << " "; 
  out << "tpc:" << hand.handshake.tpc << " "; 
  out << "brtaken:" << hand.handshake.brtaken << " "; 
  out << "ins:" << hand.handshake.ins << " ";
  out << "flags:" << hand.handshake.sleep_thread << hand.handshake.resume_thread;
  out << hand.handshake.iteration_correction << hand.handshake.real;
  out << hand.handshake.in_critical_section;
  out << " slicenum:" << hand.handshake.slice_num << " ";
  out << "feederslicelen:" << hand.handshake.feeder_slice_length << " ";
  out << "feedersliceweight:" << hand.handshake.slice_weight_times_1000 << " ";
  out.flush();
  return out;
}

BufferManager::BufferManager()
{  
}

handshake_container_t* BufferManager::front(THREADID tid)
{
  checkFirstAccess(tid);
  assert(handshake_buffer_[tid].size() > 0);
  return handshake_buffer_[tid].front();
}

handshake_container_t* BufferManager::back(THREADID tid)
{
  checkFirstAccess(tid);
  assert(handshake_buffer_[tid].size() > 0);
  return handshake_buffer_[tid].back();
}

bool BufferManager::empty(THREADID tid)
{
  checkFirstAccess(tid);    
  return handshake_buffer_[tid].size() == 0;
}

void BufferManager::push(THREADID tid, handshake_container_t* handshake, bool fromILDJIT)
{
  //  (*(logs_[tid])) << "pushing:" << (*handshake) << " with old size:" << handshake_buffer_[tid].size() << " " << fromILDJIT << endl;
  checkFirstAccess(tid);    
  
  handshake_container_t* free = getPooledHandshake(tid, fromILDJIT);
  
  free->valid = handshake->valid;
  free->mem_released = handshake->mem_released;
  free->mem_buffer = handshake->mem_buffer;
  //  free->isFirstInsn = handshake->isFirstInsn;
  free->isLastInsn = handshake->isLastInsn;
  free->killThread = handshake->killThread;
  memcpy(&(free->handshake), &(handshake->handshake), sizeof(P2Z_HANDSHAKE));
  
  handshake_buffer_[tid].push(free);
}

void BufferManager::pop(THREADID tid, handshake_container_t* handshake)
{
  //  (*(logs_[tid])) << "popping:" << handshake << ":" << (*handshake) << " with old size:" << handshake_buffer_[tid].size() << endl;
  checkFirstAccess(tid);

  assert(handshake_buffer_[tid].size() > 0);
  handshake_buffer_[tid].pop();
  releasePooledHandshake(tid, handshake);
}

bool BufferManager::hasThread(THREADID tid) 
{
  return (handshake_buffer_.count(tid) != 0);
}

unsigned int BufferManager::size()
{
  return handshake_buffer_.size();
}

void BufferManager::checkFirstAccess(THREADID tid)
{
  if(handshake_buffer_.count(tid) == 0) {
    
    PIN_Yield();
    sleep(5);
    PIN_Yield();
    
    for (int i=0; i < 250000; i++) {
      handshake_container_t* new_handshake = new handshake_container_t();
      if (i > 0) {
	new_handshake->isFirstInsn = false;            
      }
      else {
	new_handshake->isFirstInsn = true;
      }
      handshake_pool_[tid].push(new_handshake);      
    }

    locks_[tid] = new pthread_mutex_t();
    pthread_mutex_init(locks_[tid], 0);
    
    char s_tid[100];
    sprintf(s_tid, "%d", tid);

    string logName = "./output_ring_cache/handshake_" + string(s_tid) + ".log"; 
    logs_[tid] = new ofstream();
    (*(logs_[tid])).open(logName.c_str());    
  }
}

handshake_container_t* BufferManager::getPooledHandshake(THREADID tid, bool fromILDJIT)
{
  checkFirstAccess(tid);    

  handshake_queue_t *pool;
  pool = &(handshake_pool_[tid]);

  long long int spins = 0;  
  while(pool->empty()) {
    ReleaseLock(&simbuffer_lock);
    PIN_Yield();
    GetLock(&simbuffer_lock, tid+1);
    spins++;
    if(spins >= 7000000LL) {
      cerr << tid << " [getPooledHandshake()]: That's a lot of spins!" << endl;
      spins = 0;
    }
  }

  handshake_container_t* result = pool->front();
  ASSERTX(result != NULL);

  pool->pop();
  
  if((didFirstInsn_.count(tid) == 0) && (!fromILDJIT)) {    
    result->isFirstInsn = true;
    didFirstInsn_[tid] = true;
  }
  else {// this else might be wrong...
    result->isFirstInsn = false;
  }
  
  return result;
}

void BufferManager::releasePooledHandshake(THREADID tid, handshake_container_t* handshake)
{
  handshake_pool_[tid].push(handshake);
  return;
}

bool BufferManager::isFirstInsn(THREADID tid)
{
  bool isFirst = (didFirstInsn_.count(tid) == 0);
  //  didFirstInsn_[tid] = true;
  return isFirst;
}
