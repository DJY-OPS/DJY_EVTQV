#ifndef FRONT_CLI_H
#define FRONT_CLI_H

/* Non-blocking bench CLI on USART2/ST-Link VCP, 115200 8N1.
 * This is a commissioning tool, not the final steering-wheel input device. */
void FrontCli_Init(void);
void FrontCli_Process(void);

#endif /* FRONT_CLI_H */
