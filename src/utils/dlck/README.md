# DAOS Local Consistency Checker

## Description

DAOS Local Consistency Checker (DLCK) is a part of Catastrophic Recovery for DAOS dedicated to checking for corruption occurring on the `daos_engine` level. It includes corruptions in `sys_db` and VOS pools (metadata, DTX records, VEA, RDB). If requested, DLCK allows also fixing a defined set of corruptions with a focus on minimising data loss.

**Note**: "Fixing" some corruptions is not possible without data loss.

## Usage

WIP

## Design

DLCK is delivered as an executable that accepts a command from the user, specifying the type of operation to be performed, along with a path to the files on which the operation will be executed. If DLCK detects corruption, it should not crash, as `daos_engine` would; instead, it will describe the nature of the corruption to the user and prompt them to decide whether and how to address the issue.

DLCK builds on existing components such as VOS and COMMON (including BTREE, MEM, etc.). Whenever possible, DLCK utilizes existing APIs to minimize code redundancy; however, it also requires new dedicated APIs (*) and additional instrumentation (**) for the existing APIs.

```
┌──────────────┐        
│     DLCK     │        
├──────────────┤        
│     VOS**    │        
├──────┬───┬───┤        
│      │   │   │        
│BTREE*│MEM│...│  COMMON
│      │   │   │        
└──────┴───┴───┘          
```

Due to the introduced instrumentation, the instrumented modules (such as VOS) are not guaranteed to perform or behave exactly as they normally would. Therefore, dedicated copies of these components are provided for DLCK.

Given the nature of DLCK's purpose, it is essential to be acutely aware of when checks and dereferencing occur. This is why the instrumentation is carefully placed within the source code, and the finely-tuned check functions are positioned adjacent to the online functions. This approach ensures that both implementations can be relatively easy to synchronize, as illustrated in the example below.


```c
#ifdef DLCK_ENABLED
static int
online_function_check(umem_off_t *off, child_type_df **child_ptr)
{
        child_type_df *child = *child_ptr;
        // Check whether off is in range of valid offsets.
        if (child->magic != CHILD_MAGIC) {
                // Report to the user and fix or return an error.
        }
        return DER_SUCCESS;
}
#endif /* DLCK_ENABLED */

int
online_function(parent_type_df *parent)
{
        child_type_df *child = umem_off2ptr(umm, parent->child_off);
        DLCK_CALL_CHECK_RETURN(online_function_check, &parent->child_off, &child);
        D_ASSERT(child->magic == CHILD_MAGIC);
        // ...
}
```

## Btree's validation

Since a B-tree is expected to be always balanced, it is not feasible to check and fix B-tree nodes individually. In such cases, re-balancing may encounter another corrupted node, potentially causing DLCK to crash.

Therefore, DLCK checks the entire B-tree at once, identifies faulty nodes, and reports them back to the user. If the user decides to proceed with the fixes, DLCK will correct all the nodes and re-balance the tree.
