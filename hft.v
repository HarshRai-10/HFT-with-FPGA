module hft #(
    parameter N_SHORT = 12,
    parameter N_LONG  = 26,
    parameter N_RSI = 14
)(
    //primary inputs
    input  wire        s_aclk,
    input  wire        s_aresetn,
    input  wire        s_axis_tvalid,
    input  wire        m_axis_tready,
    input  wire        s_axis_tlast,
    input  wire [31:0] s_axis_tdata,

    //intermediate values for divider
    output reg [31:0]  numerator_out,
    output reg [31:0]  denominator_out,
    output reg s_axis_divisor_tvalid,
    output reg s_axis_dividend_tvalid,
    output reg m_axis_dout_tready,

    //divider inputs
    input wire [31:0] m_axis_dout_tdata,
    input wire m_axis_dout_tvalid,
    input wire m_axis_divisor_tready,
    input wire m_axis_dividend_tready,

    //final outputs
    output reg         s_axis_tready,
    output reg         m_axis_tvalid,
    output reg         m_axis_tlast,
    output reg [31:0]  m_axis_macd_tdata,
    output reg [31:0]  m_axis_rsi_tdata
);

    // MACD buffers
    reg [31:0] samples_short [0:N_SHORT-1];
    reg [31:0] samples_long  [0:N_LONG-1];
    reg [$clog2(N_SHORT)-1:0] wr_ptr_short;
    reg [$clog2(N_LONG)-1:0]  wr_ptr_long;

    //RSI buffers
    reg [31:0]  prev_price;
    reg [31:0]  gains [0:N_RSI-1];
    reg [31:0]  losses[0:N_RSI-1];
    reg [$clog2(N_RSI)-1:0] wr_ptr;
    reg [31:0]  delta;
    reg [31:0]  avg_gain, avg_loss;
    reg [31:0]  numerator, denominator;

    // Sums
    reg [31+$clog2(N_SHORT):0] sum_short;
    reg [31+$clog2(N_LONG):0]  sum_long;
    reg [31+$clog2(N_RSI):0]  gain_sum;
    reg [31+$clog2(N_RSI):0]  loss_sum;

    reg [31:0] inData;
    reg isLast;
    integer i;

    // FSM states
    localparam  IDLE       = 0,
                READ       = 1,
                RSI_DELTA  = 2,
                RSI_UPDATE = 3,
                RSI_AVG    = 4,
                RSI_RATIO  = 5,
                RSI_OUTPUT = 6,
                COMPUTE    = 7,
                DIVISOR_WAIT = 8,
                DIVIDEND_WAIT = 9,
                DIVIDER    = 10,
                WRITE      = 11;

    reg [3:0] state;

    always @(posedge s_aclk) begin
        if (!s_aresetn) begin
            wr_ptr_short <= 0;
            wr_ptr_long  <= 0;
            sum_short    <= 0;
            sum_long     <= 0;
            s_axis_tready <= 0;
            m_axis_macd_tdata <= 0;
            m_axis_rsi_tdata <= 0;
            m_axis_tvalid <= 0;
            m_axis_tlast  <= 0;
            wr_ptr <= 0;
            gain_sum <= 0;
            loss_sum <= 0;
            prev_price <= 0;
            numerator_out <= 0;
            denominator_out <= 0;
            s_axis_divisor_tvalid <= 0;
            s_axis_dividend_tvalid <= 0;
            m_axis_dout_tready <= 0;
            state <= IDLE;

            for (i = 0; i < N_SHORT; i = i+1) samples_short[i] <= 0;
            for (i = 0; i < N_LONG;  i = i+1) samples_long[i]  <= 0;
            for (i = 0; i < N_RSI; i = i+1) begin
                gains[i] <= 0;
                losses[i] <= 0;
            end

        end else begin
            case (state)



                IDLE: begin
                    m_axis_tvalid <= 0;
                    if (s_axis_tvalid) begin
                        s_axis_tready <= 1;
                        inData <= s_axis_tdata;
                        isLast <= s_axis_tlast;
                        state  <= READ;
                    end
                end



                READ: begin
                    s_axis_tready <= 0;

                    sum_short <= sum_short - samples_short[wr_ptr_short] + inData;
                    sum_long  <= sum_long  - samples_long[wr_ptr_long]  + inData;
                    samples_short[wr_ptr_short] <= inData;
                    samples_long[wr_ptr_long]   <= inData;
                    wr_ptr_short <= (wr_ptr_short == N_SHORT-1) ? 0 : wr_ptr_short + 1;
                    wr_ptr_long  <= (wr_ptr_long  == N_LONG-1)  ? 0 : wr_ptr_long  + 1;

                    state <= RSI_DELTA;
                end



                RSI_DELTA: begin
                    if (prev_price != 0) begin
                        if (inData > prev_price)
                            delta <= inData - prev_price;
                        else
                            delta <= prev_price - inData;
                    end
                    state <= RSI_UPDATE;
                end



                RSI_UPDATE: begin
                    if (prev_price != 0) begin
                        if (inData > prev_price) begin
                            gain_sum <= gain_sum - gains[wr_ptr] + delta;
                            loss_sum <= loss_sum - losses[wr_ptr];
                            gains[wr_ptr] <= delta;
                            losses[wr_ptr] <= 0;
                        end else begin
                            gain_sum <= gain_sum - gains[wr_ptr];
                            loss_sum <= loss_sum - losses[wr_ptr] + delta;
                            gains[wr_ptr] <= 0;
                            losses[wr_ptr] <= delta;
                        end
                    end

                    wr_ptr <= (wr_ptr == N_RSI-1) ? 0 : wr_ptr + 1;

                    state <= RSI_AVG;
                end

                

                RSI_AVG: begin
                    avg_gain <= (gain_sum * 73) / 1024;
                    avg_loss <= (loss_sum * 73) / 1024;
                    prev_price <= inData;
                    state <= RSI_RATIO;
                end


                             
                RSI_RATIO: begin
                if (avg_loss == 0) begin
                    numerator  <= 100;
                    denominator <= 1;
                end else begin
                    numerator  <= 100 * avg_gain;
                    denominator <= avg_loss + avg_gain;
                end

                state <= RSI_OUTPUT;
                end

                

                RSI_OUTPUT: begin
                m_axis_macd_tdata <= (sum_short * 85 / 1024) - (sum_long * 39 / 1024);

                numerator_out   <= numerator;
                denominator_out <= denominator;
                state <= COMPUTE;
                end


                COMPUTE: begin

                    s_axis_divisor_tvalid <= 1;
                    s_axis_dividend_tvalid <= 1;

                    if (m_axis_divisor_tready && m_axis_dividend_tready)
                        state <= DIVIDER;
                    else if(m_axis_dividend_tready)
                        state <= DIVISOR_WAIT;
                    else if(m_axis_divisor_tready)
                        state <= DIVIDEND_WAIT;
                end

                DIVISOR_WAIT: begin
                    s_axis_dividend_tvalid <= 0;
                    if(m_axis_divisor_tready) begin
                        state <= DIVIDER;
                        end
                end

                DIVIDEND_WAIT: begin
                    s_axis_divisor_tvalid <= 0;
                    if(m_axis_dividend_tready) begin
                        state <= DIVIDER;
                        end
                end

                DIVIDER: begin
                    s_axis_divisor_tvalid <= 0;
                    s_axis_dividend_tvalid <= 0;

                    if (m_axis_dout_tvalid) begin
                        m_axis_dout_tready <= 1;
                        m_axis_rsi_tdata <= m_axis_dout_tdata;
                        m_axis_tlast <= isLast;
                        state <= WRITE;
                    end
                end

                WRITE: begin
                    m_axis_dout_tready <= 0;
                    m_axis_tvalid <= 1;

                    if (m_axis_tready)
                        state <= IDLE;
                end
            endcase
        end
    end
endmodule
