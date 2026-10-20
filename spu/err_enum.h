typedef enum status_code {
  STATUS_OK          = 0,
  FILE_OPEN_ERR      = -1,
  FILE_STAT_ERR      = -2,
  ALLOCATION_ERR     = -3,
  FILE_READ_ERR      = -4,
  INVALID_REG_INDEX  = -5,
  INVALID_JMP_OFFSET = -6,
  INVALID_OPCODE     = -7,
} status_code;
